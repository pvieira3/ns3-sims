/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author: Pete Vieira <pete.vieira@gatech.edu>
 * Date: Jan 2014
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   * Neither the name of the Humanoid Robotics Lab nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

// NS3 Includes
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/uinteger.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/random-variable-stream.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Project_02-Comparison of RED vs. DropTail Queuing");

// Network topology (TCP/IP Protocol)
//                   q1
//
//               15 Mbs                                10 Mbps 
//       UDP   l4 ----                                   ----- r7   UDP
//                   |                                   |
//                   |                           q2      |
//                   |                                   |        
// Node: TCP   l5 -- c3 -------- c0 --------- c1 ----  c2 -- r8   TCP
// Bandwidth:        |   10 Mbp        10 Mbps    1 Mbps |
//                   |                                   |
//                   |                                   |      
//       TCP   l6 ---                                    ---- r9    TCP


//
// - Flow from n0 to n3 using BulkSendApplication
// - Receipt of bulk send at n3 using PacketSinkApplication
// - Output trace file to p1.tr
// - Vary DropTail queue size (30, 60, 120, 190, 240)
//        RED minTh \ (queue length threshold to trigger probabilistic drops) 5 15 30 60 120
//            maxTh \ (queue length threshold to trigger forced drops)        15 45 90 180 360
//            maxP  \ (max probability of doing an early drop)                1/20 1/10 1/4
//            Wq    \ (weighting factor for average queue length computation) 1/128 1/256 1/512
//            qlen  \ (max # of packets that can be enqueued)                 480
// - Compare goodput, which we assume is proportional to response time

int main (int argc, char *argv[])
{
  std::cout << "\n----------------------------" << std::endl;
  std::cout << "    Running Simulation P2   " << std::endl;
  std::cout << "----------------------------" << std::endl;
  Time::SetResolution (Time::NS);
  // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
 
  // Set default values for simulation variables
  std::string dataFileName= "p2.data";
  std::string animFile= "p2-anim.xml";
  std::string tcpType = "TcpTahoe";
  uint32_t numNodes = 10;
  // uint32_t numLeaf1Nodes = 3;
  // uint32_t numLeaf2Nodes = 2;
  // uint32_t numUdpLeaves = 1;
  uint32_t winSize    = 2000;
  uint32_t queSize    = 2000;
  uint32_t segSize    = 512;
  uint32_t maxBytes   = 100000000;
  uint32_t maxP;
  std::string queueType = "DropTail";
  // RED Queue attributes
  double minTh;
  double maxTh;
  double Wq;
  uint32_t qlen;

  // Parse command line arguments
  CommandLine cmd;
  cmd.AddValue ("dataFileName",   "Data file name",                      dataFileName);
  cmd.AddValue ("animFile",   "Animation file name",                     animFile);
  cmd.AddValue ("tcpType",    "TCP type (use TcpReno or TcpTahoe)",      tcpType);
  cmd.AddValue ("winSize",    "Receiver advertised window size (bytes)", winSize);
  cmd.AddValue ("queSize",    "Queue limit on the bottleneck link",      queSize);
  cmd.AddValue ("segSize",    "TCP segment size",                        segSize);
  cmd.AddValue ("maxBytes",   "Max bytes soure will send",               maxBytes);
  // RED Parameters
  cmd.AddValue ("queueType",  "Set Queue type to DropTail or RED",                       queueType);
  cmd.AddValue ("minTh",      "Queue length threshold to trigger probabilistic drops",   minTh);
  cmd.AddValue ("maxTh",      "Queue length threshold to trigger forced drops",          maxTh);
  cmd.AddValue ("maxP",       "Max probability of doing an early drop",                  maxP);
  cmd.AddValue ("Wq",         "Weighting factor for average queue length computation",   Wq);
  cmd.AddValue ("qlen",       "Max number of bytes that can be enqueued",                qlen);
  cmd.Parse(argc, argv);

  // Set default values
  // DropTail Queue defaults
  Config::SetDefault ("ns3::DropTailQueue::Mode", EnumValue(DropTailQueue::QUEUE_MODE_BYTES));
  Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue(queSize));
  // RED Queue defaults
  Config::SetDefault ("ns3::RedQueue::Mode", EnumValue(RedQueue::QUEUE_MODE_BYTES));
  Config::SetDefault ("ns3::RedQueue::MinTh", DoubleValue (minTh));
  Config::SetDefault ("ns3::RedQueue::MaxTh", DoubleValue (maxTh));
  Config::SetDefault ("ns3::RedQueue::QW", DoubleValue(Wq));
  Config::SetDefault ("ns3::RedQueue::QueueLimit", UintegerValue (qlen));
  // TCP defaults
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpTahoe"));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(winSize));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(segSize));
  // UDP defaults

  // Check Queue type user input
  if (queueType == "RED") {
    queueType = "ns3::RedQueue";
  } else if (queueType == "DropTail") {
    queueType = "ns3::DropTailQueue";
  } else {
    NS_ABORT_MSG ("Invalid queue type: Use --queueType=RED or --queueType=DropTail");
  }


  //---------------------------------------------------------------
  //                     CREATE TOPOLOGY
  //---------------------------------------------------------------
  // 1. Create point to point helpers for each set of links
  // 2. Create NodeContainers and add nodes to them
  // 3. Create NetDeviceContainers and install the nodes onto them

  //------------------------
  //      CREATE NODES
  //------------------------
  // Center routers
  NodeContainer n;
  n.Create(numNodes);


  //------------------------
  //      CREATE LINKS
  //------------------------
  // Router links
  NodeContainer c0c1 = NodeContainer(n.Get(0), n.Get(1));
  NodeContainer c1c2 = NodeContainer(n.Get(1), n.Get(2));
  NodeContainer c0c3 = NodeContainer(n.Get(0), n.Get(3));

  // Left links
  NodeContainer c3l4 = NodeContainer(n.Get(3), n.Get(4));
  NodeContainer c3l5 = NodeContainer(n.Get(3), n.Get(5));
  NodeContainer c3l6 = NodeContainer(n.Get(3), n.Get(6));

  // Right links
  NodeContainer c2r7 = NodeContainer(n.Get(2), n.Get(7));
  NodeContainer c2r8 = NodeContainer(n.Get(2), n.Get(8));
  NodeContainer c2r9 = NodeContainer(n.Get(2), n.Get(9));

  // Vector of TCP links
  NodeContainer routerNodes;
  routerNodes.Add(n.Get(3));
  routerNodes.Add(n.Get(0));
  routerNodes.Add(n.Get(1));
  routerNodes.Add(n.Get(2));

  NodeContainer leftNodes;
  leftNodes.Add(n.Get(4));
  leftNodes.Add(n.Get(5));
  leftNodes.Add(n.Get(6));

  // Vector UDF links
  NodeContainer rightNodes;
  rightNodes.Add(n.Get(7));
  rightNodes.Add(n.Get(8));
  rightNodes.Add(n.Get(9));
  

  //----------------------------------
  //     PLACE NODES FOR ANIMATION
  //----------------------------------
  // Add router node locations
  for(uint32_t i=0; i<routerNodes.GetN(); ++i) {
    Ptr<Node> node = routerNodes.Get(i);
    Ptr<ConstantPositionMobilityModel> loc = node->GetObject<ConstantPositionMobilityModel>();
    loc = CreateObject<ConstantPositionMobilityModel>();
    node->AggregateObject(loc);
    Vector locVec(i, 0, 0);
    loc->SetPosition(locVec);
  }
  // Add left node locations
  for(uint32_t i=0; i<leftNodes.GetN(); ++i) {
    Ptr<Node> node = leftNodes.Get(i);
    Ptr<ConstantPositionMobilityModel> loc = node->GetObject<ConstantPositionMobilityModel>();
    loc = CreateObject<ConstantPositionMobilityModel>();
    node->AggregateObject(loc);
    Vector locVec(-1, 1 + i, 0);
    loc->SetPosition(locVec);
  }
  // Add right node locations
  for(uint32_t i=0; i<rightNodes.GetN(); ++i) {
    Ptr<Node> node = rightNodes.Get(i);
    Ptr<ConstantPositionMobilityModel> loc = node->GetObject<ConstantPositionMobilityModel>();
    loc = CreateObject<ConstantPositionMobilityModel>();
    node->AggregateObject(loc);
    Vector locVec(4, 1 + i, 0);
    loc->SetPosition(locVec);
  }


  //------------------------
  //     CREATE DEVICES
  //------------------------
  // Bottleneck 1
  PointToPointHelper bottleneck1;
  bottleneck1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  bottleneck1.SetChannelAttribute ("Delay", StringValue ("5ms"));
  bottleneck1.SetQueue (queueType);
                        
  NetDeviceContainer dc0c3 = bottleneck1.Install(c0c3);

  // Bottleneck 2
  PointToPointHelper bottleneck2;
  bottleneck2.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  bottleneck2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  bottleneck2.SetQueue (queueType);
  NetDeviceContainer dc1c2 = bottleneck2.Install(c1c2);

  // Center routers
  PointToPointHelper center;
  center.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  center.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer dc0c1 = center.Install(c0c1);

  // Left leaves
  PointToPointHelper p2pLeft;
  p2pLeft.SetDeviceAttribute ("DataRate", StringValue ("15Mbps"));
  p2pLeft.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer dc3l4 = p2pLeft.Install(c3l4);
  NetDeviceContainer dc3l5 = p2pLeft.Install(c3l5);
  NetDeviceContainer dc3l6 = p2pLeft.Install(c3l6);

  // Right leaves
  PointToPointHelper p2pRight;
  p2pRight.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pRight.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer dc2r7 = p2pRight.Install(c2r7);
  NetDeviceContainer dc2r8 = p2pRight.Install(c2r8);
  NetDeviceContainer dc2r9 = p2pRight.Install(c2r9);
  
  // Create vector of NetDeviceContainer to loop through when assigning ip addresses
  // NetDeviceContainer devsArray[] = {dc1c2, dc0c1, dc3l4, dc3l5, dc3l6, dc2r7, dc2r8, dc2r9};
  // std::vector<NetDeviceContainer> devices(devsArray, devsArray + sizeof(devsArray) / sizeof(NetDeviceContainer));
  std::vector<NetDeviceContainer> devices;
  devices.push_back(dc0c3);
  devices.push_back(dc1c2);
  devices.push_back(dc0c1);
  devices.push_back(dc3l4);
  devices.push_back(dc3l5);
  devices.push_back(dc3l6);
  devices.push_back(dc2r7);
  devices.push_back(dc2r8);
  devices.push_back(dc2r9);

  //-------------------------
  //    ADD INTERNET STACK
  //-------------------------
  // Add TCP/IP stack to all nodes (Transmission Control Protocol / Internet Protocol)
  // Application (data): Encodes the data being sent.
  // Transport (UDP)   : Splits data into manageable chunks, adds port # info.
  // Internet (IP)     : Adds IP addresses stating where data is from and going.
  // Link (frame)      : Adds MAC address info to tell which HW device the message
  //                     is from and which HW device it is going to.
  std::cerr << "Installing internet stack" << std::endl;
  InternetStackHelper stack;
  stack.Install (n);
  

  //------------------------
  //    ADD IP ADDRESSES
  //------------------------
  // Hardware is in place. Now assign IP addresses
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> ifaceLinks(numNodes-1);
  for(uint32_t i=0; i<devices.size(); ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i+1 << ".0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    ifaceLinks[i] = ipv4.Assign(devices[i]);
  }

  // Set port
  uint16_t port = 9;


  //---------------------------
  //    INSTALL SOURCE APPS
  //---------------------------
  // TCP On/Off 1
  OnOffHelper onOffTcp1("ns3::TcpSocketFactory", InetSocketAddress(ifaceLinks[3].GetAddress(1)));
  onOffTcp1.SetConstantRate(DataRate("2kbps"));
  onOffTcp1.SetAttribute("PacketSize", UintegerValue(50));
  // TCP On/Off 2
  OnOffHelper onOffTcp2("ns3::TcpSocketFactory", InetSocketAddress(ifaceLinks[4].GetAddress(1)));
  onOffTcp2.SetConstantRate(DataRate("2kbps"));
  onOffTcp2.SetAttribute("PacketSize", UintegerValue(50));
  // UDP On/Off 1
  OnOffHelper onOffUdp1("ns3::UdpSocketFactory", InetSocketAddress(ifaceLinks[5].GetAddress(1)));
  onOffUdp1.SetConstantRate(DataRate("2kbps"));
  onOffUdp1.SetAttribute("PacketSize", UintegerValue(50));
  // Add source apps
  ApplicationContainer sourceApps;
  sourceApps.Add(onOffTcp1.Install(c3l5.Get(1)));
  sourceApps.Add(onOffTcp2.Install(c3l6.Get(1)));
  sourceApps.Add(onOffUdp1.Install(c3l4.Get(1)));


  //---------------------------
  //    INSTALL SINK APPS
  //---------------------------
  // Create Udp sink app and install it on node r7
  PacketSinkHelper sinkUpd1("ns3::UdpSocketFactory",
                            Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  // Create Tcp sink apps and install them on nodes r8 & r9
  PacketSinkHelper sinkTcp1("ns3::TcpSocketFactory",
                            Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  PacketSinkHelper sinkTcp2("ns3::TcpSocketFactory",
                            Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  // Add sink apps
  ApplicationContainer sinkApps;
  sinkApps.Add(sinkTcp1.Install(c2r8.Get(1)));
  sinkApps.Add(sinkTcp2.Install(c2r9.Get(1)));
  sinkApps.Add(sinkUpd1.Install(c2r7.Get(1)));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));


  //------------------------------
  //    NETANIM ANIMATION SETUP
  //------------------------------
  // Animation setup and bounding box for animation
  // dumbbell.BoundingBox (1, 1, 100, 100);
  AnimationInterface animInterface(animFile);
  animInterface.EnablePacketMetadata(true);
  std::cerr << "\nSaving animation file: " << animFile << std::endl;


  //------------------------------
  //    POPULATE ROUTING TABLES
  //------------------------------
  // Uses shortest path search from every node to every possible destination
  // to tell nodes how to route packets. A routing table is the next top route
  // to the possible routing destinations.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  //---------------------
  //    RUN SIMULATION
  //---------------------
  std::cout << "\nRuning simulation..." << std::endl;
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  std::cout << "\nSimulation finished!" << std::endl;


  //------------------------
  //    COMPUTE GOODPUT
  //------------------------
  // Print out Goodput of the network communication from source to sink
  // Goodput: Amount of useful information (bytes) per unit time (seconds)
  std::vector<uint32_t> goodputs;
  uint32_t i = 0;
  for(ApplicationContainer::Iterator it = sinkApps.Begin(); it != sinkApps.End(); ++it) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (*it);
    uint32_t bytesRcvd = sink->GetTotalRx ();
    goodputs.push_back(bytesRcvd / 10.0);
    std::cout << "\nFlow " << i << ":" << std::endl;
    std::cout << "\tTotal Bytes Received: " << bytesRcvd << std::endl;
    std::cout << "\tGoodput: " << goodputs.back() << " Bytes/seconds" << std::endl;
    ++i;
  }


  //-------------------------
  //    WRITE DATA TO FILE
  //-------------------------
  // Write results to data file
  // std::ofstream dataFile;
  // dataFile.open(dataFileName.c_str(), std::fstream::out | std::fstream::app);
  // dataFile << tcpType << "\t"
  //          << winSize << "\t"
  //          << queSize << "\t"
  //          << segSize;
  // for(std::vector<uint32_t>::iterator gp = goodputs.begin(); gp != goodputs.end(); ++gp) {
  //   dataFile << "\t" << *gp;
  // }
  // dataFile  << "\n";
  // dataFile.close();


  // return
  return 0;
}
