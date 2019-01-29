// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/testing/fake_mdns_responder_adapter.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

constexpr char kTestServiceInstance[] = "turtle";
constexpr char kTestServiceName[] = "_foo";
constexpr char kTestServiceProtocol[] = "_udp";

platform::UdpSocketPtr default_socket =
    reinterpret_cast<platform::UdpSocketPtr>(8);

TEST(FakeMdnsResponderAdapterTest, AQueries) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());
  auto event = MakeAEvent("alpha", IPAddress{1, 2, 3, 4}, default_socket);
  auto domain_name = event.domain_name;
  mdns_responder.AddAEvent(std::move(event));

  auto a_events = mdns_responder.TakeAResponses();
  EXPECT_TRUE(a_events.empty());

  auto result = mdns_responder.StartAQuery(default_socket, domain_name);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  a_events = mdns_responder.TakeAResponses();
  ASSERT_EQ(1u, a_events.size());
  EXPECT_EQ(domain_name, a_events[0].domain_name);
  EXPECT_EQ((IPAddress{1, 2, 3, 4}), a_events[0].address);

  result = mdns_responder.StopAQuery(default_socket, domain_name);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());

  mdns_responder.AddAEvent(
      MakeAEvent("alpha", IPAddress{1, 2, 3, 4}, default_socket));
  result = mdns_responder.StartAQuery(default_socket, domain_name);
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  a_events = mdns_responder.TakeAResponses();
  EXPECT_TRUE(a_events.empty());
}

TEST(FakeMdnsResponderAdapterTest, AaaaQueries) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());
  auto event = MakeAaaaEvent("alpha", IPAddress{1, 2, 3, 4}, default_socket);
  auto domain_name = event.domain_name;
  mdns_responder.AddAaaaEvent(std::move(event));

  auto aaaa_events = mdns_responder.TakeAaaaResponses();
  EXPECT_TRUE(aaaa_events.empty());

  auto result = mdns_responder.StartAaaaQuery(default_socket, domain_name);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  aaaa_events = mdns_responder.TakeAaaaResponses();
  ASSERT_EQ(1u, aaaa_events.size());
  EXPECT_EQ(domain_name, aaaa_events[0].domain_name);
  EXPECT_EQ((IPAddress{1, 2, 3, 4}), aaaa_events[0].address);

  result = mdns_responder.StopAaaaQuery(default_socket, domain_name);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());

  mdns_responder.AddAaaaEvent(
      MakeAaaaEvent("alpha", IPAddress{1, 2, 3, 4}, default_socket));
  result = mdns_responder.StartAaaaQuery(default_socket, domain_name);
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  aaaa_events = mdns_responder.TakeAaaaResponses();
  EXPECT_TRUE(aaaa_events.empty());
}

TEST(FakeMdnsResponderAdapterTest, PtrQueries) {
  const mdns::DomainName kTestServiceType{
      {4, '_', 'f', 'o', 'o', 4, '_', 'u', 'd', 'p', 0}};
  const mdns::DomainName kTestServiceTypeCanon{{4, '_', 'f', 'o', 'o', 4, '_',
                                                'u', 'd', 'p', 5, 'l', 'o', 'c',
                                                'a', 'l', 0}};

  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());
  mdns_responder.AddPtrEvent(
      MakePtrEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   default_socket));

  auto ptr_events = mdns_responder.TakePtrResponses();
  EXPECT_TRUE(ptr_events.empty());

  auto result = mdns_responder.StartPtrQuery(default_socket, kTestServiceType);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  ptr_events = mdns_responder.TakePtrResponses();
  ASSERT_EQ(1u, ptr_events.size());
  auto labels = ptr_events[0].service_instance.GetLabels();
  EXPECT_EQ(kTestServiceInstance, labels[0]);

  // TODO(btolsch): qname if PtrEvent gets it.
  ErrorOr<mdns::DomainName> st =
      mdns::DomainName::FromLabels(labels.begin() + 1, labels.end());
  ASSERT_TRUE(st);
  EXPECT_EQ(kTestServiceTypeCanon, st.value());

  result = mdns_responder.StopPtrQuery(default_socket, kTestServiceType);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());

  mdns_responder.AddPtrEvent(
      MakePtrEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   default_socket));
  result = mdns_responder.StartPtrQuery(default_socket, kTestServiceType);
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  ptr_events = mdns_responder.TakePtrResponses();
  EXPECT_TRUE(ptr_events.empty());
}

TEST(FakeMdnsResponderAdapterTest, SrvQueries) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());

  auto event =
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "alpha", 12345, default_socket);
  auto service_instance = event.service_instance;
  auto domain_name = event.domain_name;
  mdns_responder.AddSrvEvent(std::move(event));

  auto srv_events = mdns_responder.TakeSrvResponses();
  EXPECT_TRUE(srv_events.empty());

  auto result = mdns_responder.StartSrvQuery(default_socket, service_instance);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  srv_events = mdns_responder.TakeSrvResponses();
  ASSERT_EQ(1u, srv_events.size());
  EXPECT_EQ(service_instance, srv_events[0].service_instance);
  EXPECT_EQ(domain_name, srv_events[0].domain_name);
  EXPECT_EQ(12345, srv_events[0].port);

  result = mdns_responder.StopSrvQuery(default_socket, service_instance);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());

  mdns_responder.AddSrvEvent(
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "alpha", 12345, default_socket));
  result = mdns_responder.StartSrvQuery(default_socket, service_instance);
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  srv_events = mdns_responder.TakeSrvResponses();
  EXPECT_TRUE(srv_events.empty());
}

TEST(FakeMdnsResponderAdapterTest, TxtQueries) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());

  const auto txt_lines = std::vector<std::string>{"asdf", "jkl;", "j"};
  auto event = MakeTxtEvent(kTestServiceInstance, kTestServiceName,
                            kTestServiceProtocol, txt_lines, default_socket);
  auto service_instance = event.service_instance;
  mdns_responder.AddTxtEvent(std::move(event));

  auto txt_events = mdns_responder.TakeTxtResponses();
  EXPECT_TRUE(txt_events.empty());

  auto result = mdns_responder.StartTxtQuery(default_socket, service_instance);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  txt_events = mdns_responder.TakeTxtResponses();
  ASSERT_EQ(1u, txt_events.size());
  EXPECT_EQ(service_instance, txt_events[0].service_instance);
  EXPECT_EQ(txt_lines, txt_events[0].txt_info);

  result = mdns_responder.StopTxtQuery(default_socket, service_instance);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());

  mdns_responder.AddTxtEvent(
      MakeTxtEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   txt_lines, default_socket));
  result = mdns_responder.StartTxtQuery(default_socket, service_instance);
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  txt_events = mdns_responder.TakeTxtResponses();
  EXPECT_TRUE(txt_events.empty());
}

TEST(FakeMdnsResponderAdapterTest, RegisterServices) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());

  auto result = mdns_responder.RegisterService(
      "instance", "name", "proto",
      mdns::DomainName{{1, 'a', 5, 'l', 'o', 'c', 'a', 'l', 0}}, 12345,
      {{"k1", "asdf"}, {"k2", "jkl"}});
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);
  EXPECT_EQ(1u, mdns_responder.registered_services().size());

  result = mdns_responder.RegisterService(
      "instance2", "name", "proto",
      mdns::DomainName{{1, 'b', 5, 'l', 'o', 'c', 'a', 'l', 0}}, 12346,
      {{"k1", "asdf"}, {"k2", "jkl"}});
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);
  EXPECT_EQ(2u, mdns_responder.registered_services().size());

  result = mdns_responder.DeregisterService("instance", "name", "proto");
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);
  result = mdns_responder.DeregisterService("instance", "name", "proto");
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  EXPECT_EQ(1u, mdns_responder.registered_services().size());

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());
  EXPECT_EQ(0u, mdns_responder.registered_services().size());

  result = mdns_responder.RegisterService(
      "instance2", "name", "proto",
      mdns::DomainName{{1, 'b', 5, 'l', 'o', 'c', 'a', 'l', 0}}, 12346,
      {{"k1", "asdf"}, {"k2", "jkl"}});
  EXPECT_NE(mdns::MdnsResponderErrorCode::kNoError, result);
  EXPECT_EQ(0u, mdns_responder.registered_services().size());
}

TEST(FakeMdnsResponderAdapterTest, RegisterInterfaces) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());
  EXPECT_EQ(0u, mdns_responder.registered_interfaces().size());

  auto socket1 = reinterpret_cast<platform::UdpSocketPtr>(16);
  auto socket2 = reinterpret_cast<platform::UdpSocketPtr>(24);
  Error result = mdns_responder.RegisterInterface(
      platform::InterfaceInfo{}, platform::IPSubnet{}, socket1);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1u, mdns_responder.registered_interfaces().size());

  result = mdns_responder.RegisterInterface(platform::InterfaceInfo{},
                                            platform::IPSubnet{}, socket1);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(1u, mdns_responder.registered_interfaces().size());

  result = mdns_responder.RegisterInterface(platform::InterfaceInfo{},
                                            platform::IPSubnet{}, socket2);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(2u, mdns_responder.registered_interfaces().size());

  result = mdns_responder.DeregisterInterface(socket2);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(1u, mdns_responder.registered_interfaces().size());
  result = mdns_responder.DeregisterInterface(socket2);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(1u, mdns_responder.registered_interfaces().size());

  mdns_responder.Close();
  ASSERT_FALSE(mdns_responder.running());
  EXPECT_EQ(0u, mdns_responder.registered_interfaces().size());

  result = mdns_responder.RegisterInterface(platform::InterfaceInfo{},
                                            platform::IPSubnet{}, socket1);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(0u, mdns_responder.registered_interfaces().size());
}

TEST(FakeMdnsResponderAdapterTest, UpdateTxtData) {
  FakeMdnsResponderAdapter mdns_responder;

  mdns_responder.Init();
  ASSERT_TRUE(mdns_responder.running());

  const std::map<std::string, std::string> txt_data1{{"k1", "asdf"},
                                                     {"k2", "jkl"}};
  auto result = mdns_responder.RegisterService(
      "instance", "name", "proto",
      mdns::DomainName{{1, 'a', 5, 'l', 'o', 'c', 'a', 'l', 0}}, 12345,
      txt_data1);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);
  ASSERT_EQ(1u, mdns_responder.registered_services().size());
  EXPECT_EQ(txt_data1, mdns_responder.registered_services()[0].txt_data);

  const std::map<std::string, std::string> txt_data2{
      {"k1", "monkey"}, {"k2", "panda"}, {"k3", "turtle"}, {"k4", "rhino"}};
  result = mdns_responder.UpdateTxtData("instance", "name", "proto", txt_data2);
  EXPECT_EQ(mdns::MdnsResponderErrorCode::kNoError, result);
  ASSERT_EQ(1u, mdns_responder.registered_services().size());
  EXPECT_EQ(txt_data2, mdns_responder.registered_services()[0].txt_data);
}

}  // namespace openscreen
