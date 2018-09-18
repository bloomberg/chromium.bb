// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/mdns_responder_service.h"

#include <memory>

#include "api/impl/screen_listener_impl.h"
#include "api/impl/testing/fake_mdns_platform_service.h"
#include "api/impl/testing/fake_mdns_responder_adapter.h"
#include "base/make_unique.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

constexpr char kTestServiceInstance[] = "turtle";
constexpr char kTestServiceName[] = "_foo";
constexpr char kTestServiceProtocol[] = "_udp";
constexpr char kTestHostname[] = "hostname";
constexpr uint16_t kTestPort = 12345;
constexpr char kTestFriendlyName[] = "towelie";

class FakeMdnsResponderAdapterFactory final
    : public MdnsResponderAdapterFactory {
 public:
  ~FakeMdnsResponderAdapterFactory() override = default;

  std::unique_ptr<mdns::MdnsResponderAdapter> Create() override {
    auto mdns = MakeUnique<FakeMdnsResponderAdapter>();
    last_mdns_responder_ = mdns.get();
    ++instances_;
    return mdns;
  }

  FakeMdnsResponderAdapter* last_mdns_responder() {
    return last_mdns_responder_;
  }

  int32_t instances() const { return instances_; }

 private:
  FakeMdnsResponderAdapter* last_mdns_responder_ = nullptr;
  int32_t instances_ = 0;
};

class MockScreenListenerObserver final : public ScreenListener::Observer {
 public:
  ~MockScreenListenerObserver() override = default;

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());
  MOCK_METHOD0(OnSearching, void());

  MOCK_METHOD1(OnScreenAdded, void(const ScreenInfo&));
  MOCK_METHOD1(OnScreenChanged, void(const ScreenInfo&));
  MOCK_METHOD1(OnScreenRemoved, void(const ScreenInfo&));
  MOCK_METHOD0(OnAllScreensRemoved, void());

  MOCK_METHOD1(OnError, void(ScreenListenerError));
  MOCK_METHOD1(OnMetrics, void(ScreenListener::Metrics));
};

class MockScreenPublisherObserver final : public ScreenPublisher::Observer {
 public:
  ~MockScreenPublisherObserver() override = default;

  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD0(OnSuspended, void());
  MOCK_METHOD1(OnError, void(ScreenPublisherError));
  MOCK_METHOD1(OnMetrics, void(ScreenPublisher::Metrics));
};

class MdnsResponderServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mdns_responder_factory = MakeUnique<FakeMdnsResponderAdapterFactory>();
    mdns_responder_factory_ = mdns_responder_factory.get();
    auto platform_service = MakeUnique<FakeMdnsPlatformService>();
    fake_platform_service_ = platform_service.get();
    fake_platform_service_->set_interfaces(bound_interfaces_);
    mdns_service_ = MakeUnique<MdnsResponderService>(
        kTestServiceName, kTestServiceProtocol,
        std::move(mdns_responder_factory), std::move(platform_service));
    screen_listener_ =
        MakeUnique<ScreenListenerImpl>(&observer_, mdns_service_.get());

    mdns_service_->SetServiceConfig(
        kTestHostname, kTestServiceInstance, kTestPort, {},
        {std::string("fn=") + std::string(kTestFriendlyName)});
    screen_publisher_ = MakeUnique<ScreenPublisherImpl>(&publisher_observer_,
                                                        mdns_service_.get());
  }

  MockScreenListenerObserver observer_;
  FakeMdnsPlatformService* fake_platform_service_;
  FakeMdnsResponderAdapterFactory* mdns_responder_factory_;
  std::unique_ptr<MdnsResponderService> mdns_service_;
  std::unique_ptr<ScreenListenerImpl> screen_listener_;
  MockScreenPublisherObserver publisher_observer_;
  std::unique_ptr<ScreenPublisherImpl> screen_publisher_;
  platform::UdpSocketPtr default_socket_ =
      reinterpret_cast<platform::UdpSocketPtr>(16);
  platform::UdpSocketPtr second_socket_ =
      reinterpret_cast<platform::UdpSocketPtr>(24);
  const uint8_t default_mac_[6] = {0, 11, 22, 33, 44, 55};
  const uint8_t second_mac_[6] = {55, 33, 22, 33, 44, 77};
  std::vector<MdnsPlatformService::BoundInterface> bound_interfaces_{
      MdnsPlatformService::BoundInterface{
          platform::InterfaceInfo{1, default_mac_, "eth0",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPSubnet{IPAddress{192, 168, 3, 2}, 24}, default_socket_},
      MdnsPlatformService::BoundInterface{
          platform::InterfaceInfo{2, second_mac_, "eth1",
                                  platform::InterfaceInfo::Type::kEthernet},
          platform::IPSubnet{IPAddress{10, 0, 0, 3}, 24}, second_socket_},
  };
};

}  // namespace

TEST_F(MdnsResponderServiceTest, BasicServiceStates) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  std::string screen_id;
  EXPECT_CALL(observer_, OnScreenAdded(::testing::_))
      .WillOnce(::testing::Invoke([&screen_id](const ScreenInfo& info) {
        screen_id = info.screen_id;
        EXPECT_EQ("shifty", info.friendly_name);
        EXPECT_EQ((IPAddress{192, 168, 3, 7}), info.endpoint.address);
        EXPECT_EQ(12345, info.endpoint.port);
      }));
  mdns_service_->HandleNewEvents({});

  mdns_responder->AddAEvent(MakeAEvent(
      "gigliorononomicon", IPAddress{192, 168, 3, 8}, default_socket_));

  EXPECT_CALL(observer_, OnScreenChanged(::testing::_))
      .WillOnce(::testing::Invoke([&screen_id](const ScreenInfo& info) {
        EXPECT_EQ(screen_id, info.screen_id);
        EXPECT_EQ("shifty", info.friendly_name);
        EXPECT_EQ((IPAddress{192, 168, 3, 8}), info.endpoint.address);
        EXPECT_EQ(12345, info.endpoint.port);
      }));
  mdns_service_->HandleNewEvents({});

  auto ptr_remove = MakePtrEvent(kTestServiceInstance, kTestServiceName,
                                 kTestServiceProtocol, default_socket_);
  ptr_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddPtrEvent(std::move(ptr_remove));

  EXPECT_CALL(observer_, OnScreenRemoved(::testing::_))
      .WillOnce(::testing::Invoke([&screen_id](const ScreenInfo& info) {
        EXPECT_EQ(screen_id, info.screen_id);
      }));
  mdns_service_->HandleNewEvents({});
}

TEST_F(MdnsResponderServiceTest, NetworkInterfaceIndex) {
  auto second_socket = reinterpret_cast<platform::UdpSocketPtr>(24);
  constexpr uint8_t mac[6] = {12, 34, 56, 78, 90};
  bound_interfaces_.emplace_back(
      platform::InterfaceInfo{2, mac, "wlan0",
                              platform::InterfaceInfo::Type::kWifi},
      platform::IPSubnet{IPAddress{10, 0, 0, 2}, 24}, second_socket);
  fake_platform_service_->set_interfaces(bound_interfaces_);
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         second_socket);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_))
      .WillOnce(::testing::Invoke([](const ScreenInfo& info) {
        EXPECT_EQ(2, info.network_interface_index);
      }));
  mdns_service_->HandleNewEvents({});
}

TEST_F(MdnsResponderServiceTest, SimultaneousFieldChanges) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_));
  mdns_service_->HandleNewEvents({});

  mdns_responder->AddTxtEvent(
      MakeTxtEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   {"fn=alpha"}, default_socket_));
  mdns_responder->AddAEvent(MakeAEvent(
      "gigliorononomicon", IPAddress{192, 168, 3, 8}, default_socket_));

  EXPECT_CALL(observer_, OnScreenChanged(::testing::_))
      .WillOnce(::testing::Invoke([](const ScreenInfo& info) {
        EXPECT_EQ("alpha", info.friendly_name);
        EXPECT_EQ((IPAddress{192, 168, 3, 8}), info.endpoint.address);
      }));
  mdns_service_->HandleNewEvents({});
}

TEST_F(MdnsResponderServiceTest, SimultaneousHostAndAddressChange) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_));
  mdns_service_->HandleNewEvents({});

  mdns_responder->AddSrvEvent(
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "alpha", 12345, default_socket_));
  mdns_responder->AddAEvent(MakeAEvent(
      "gigliorononomicon", IPAddress{192, 168, 3, 8}, default_socket_));
  mdns_responder->AddAEvent(
      MakeAEvent("alpha", IPAddress{192, 168, 3, 10}, default_socket_));

  EXPECT_CALL(observer_, OnScreenChanged(::testing::_))
      .WillOnce(::testing::Invoke([](const ScreenInfo& info) {
        EXPECT_EQ((IPAddress{192, 168, 3, 10}), info.endpoint.address);
      }));
  mdns_service_->HandleNewEvents({});
}

TEST_F(MdnsResponderServiceTest, ListenerStateTransitions) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  EXPECT_CALL(observer_, OnSuspended());
  screen_listener_->Suspend();
  ASSERT_EQ(mdns_responder, mdns_responder_factory_->last_mdns_responder());
  EXPECT_FALSE(mdns_responder->running());

  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Resume();
  ASSERT_EQ(mdns_responder, mdns_responder_factory_->last_mdns_responder());
  EXPECT_TRUE(mdns_responder->running());

  EXPECT_CALL(observer_, OnStopped());
  screen_listener_->Stop();
  ASSERT_EQ(mdns_responder, mdns_responder_factory_->last_mdns_responder());

  EXPECT_CALL(observer_, OnSuspended());
  auto instances = mdns_responder_factory_->instances();
  screen_listener_->StartAndSuspend();
  EXPECT_EQ(instances + 1, mdns_responder_factory_->instances());
  mdns_responder = mdns_responder_factory_->last_mdns_responder();
  EXPECT_FALSE(mdns_responder->running());

  EXPECT_CALL(observer_, OnStopped());
  screen_listener_->Stop();
  ASSERT_EQ(mdns_responder, mdns_responder_factory_->last_mdns_responder());
}

TEST_F(MdnsResponderServiceTest, BasicServicePublish) {
  EXPECT_CALL(publisher_observer_, OnStarted());
  screen_publisher_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  auto services = mdns_responder->registered_services();
  ASSERT_EQ(1u, services.size());
  EXPECT_EQ(kTestServiceInstance, services[0].service_instance);
  EXPECT_EQ(kTestServiceName, services[0].service_name);
  EXPECT_EQ(kTestServiceProtocol, services[0].service_protocol);
  auto host_labels = services[0].target_host.GetLabels();
  ASSERT_EQ(2u, host_labels.size());
  EXPECT_EQ(kTestHostname, host_labels[0]);
  EXPECT_EQ("local", host_labels[1]);
  EXPECT_EQ(kTestPort, services[0].target_port);

  EXPECT_CALL(publisher_observer_, OnStopped());
  screen_publisher_->Stop();

  EXPECT_EQ(0u, mdns_responder->registered_services().size());
}

TEST_F(MdnsResponderServiceTest, PublisherStateTransitions) {
  EXPECT_CALL(publisher_observer_, OnStarted());
  screen_publisher_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());
  EXPECT_EQ(1u, mdns_responder->registered_services().size());

  EXPECT_CALL(publisher_observer_, OnSuspended());
  screen_publisher_->Suspend();
  EXPECT_EQ(0u, mdns_responder->registered_services().size());

  EXPECT_CALL(publisher_observer_, OnStarted());
  screen_publisher_->Resume();
  EXPECT_EQ(1u, mdns_responder->registered_services().size());

  EXPECT_CALL(publisher_observer_, OnStopped());
  screen_publisher_->Stop();
  EXPECT_EQ(0u, mdns_responder->registered_services().size());

  EXPECT_CALL(publisher_observer_, OnStarted());
  screen_publisher_->Start();
  mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());
  EXPECT_EQ(1u, mdns_responder->registered_services().size());
  EXPECT_CALL(publisher_observer_, OnSuspended());
  screen_publisher_->Suspend();
  EXPECT_EQ(0u, mdns_responder->registered_services().size());
  EXPECT_CALL(publisher_observer_, OnStopped());
  screen_publisher_->Stop();
  EXPECT_EQ(0u, mdns_responder->registered_services().size());
}

TEST_F(MdnsResponderServiceTest, PublisherObeysInterfaceWhitelist) {
  {
    mdns_service_->SetServiceConfig(
        kTestHostname, kTestServiceInstance, kTestPort, {},
        {std::string("fn=") + std::string(kTestFriendlyName)});

    EXPECT_CALL(publisher_observer_, OnStarted());
    screen_publisher_->Start();

    auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
    ASSERT_TRUE(mdns_responder);
    ASSERT_TRUE(mdns_responder->running());
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(2u, interfaces.size());
    EXPECT_EQ(default_socket_, interfaces[0].socket);
    EXPECT_EQ(second_socket_, interfaces[1].socket);

    EXPECT_CALL(publisher_observer_, OnStopped());
    screen_publisher_->Stop();
  }
  {
    mdns_service_->SetServiceConfig(
        kTestHostname, kTestServiceInstance, kTestPort, {1, 2},
        {std::string("fn=") + std::string(kTestFriendlyName)});

    EXPECT_CALL(publisher_observer_, OnStarted());
    screen_publisher_->Start();

    auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
    ASSERT_TRUE(mdns_responder);
    ASSERT_TRUE(mdns_responder->running());
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(2u, interfaces.size());
    EXPECT_EQ(default_socket_, interfaces[0].socket);
    EXPECT_EQ(second_socket_, interfaces[1].socket);

    EXPECT_CALL(publisher_observer_, OnStopped());
    screen_publisher_->Stop();
  }
  {
    mdns_service_->SetServiceConfig(
        kTestHostname, kTestServiceInstance, kTestPort, {2},
        {std::string("fn=") + std::string(kTestFriendlyName)});

    EXPECT_CALL(publisher_observer_, OnStarted());
    screen_publisher_->Start();

    auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
    ASSERT_TRUE(mdns_responder);
    ASSERT_TRUE(mdns_responder->running());
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(1u, interfaces.size());
    EXPECT_EQ(second_socket_, interfaces[0].socket);

    EXPECT_CALL(publisher_observer_, OnStopped());
    screen_publisher_->Stop();
  }
}

TEST_F(MdnsResponderServiceTest, ListenAndPublish) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());

  {
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(2u, interfaces.size());
    EXPECT_EQ(default_socket_, interfaces[0].socket);
    EXPECT_EQ(second_socket_, interfaces[1].socket);
  }

  mdns_service_->SetServiceConfig(
      kTestHostname, kTestServiceInstance, kTestPort, {2},
      {std::string("fn=") + std::string(kTestFriendlyName)});

  auto instances = mdns_responder_factory_->instances();
  EXPECT_CALL(publisher_observer_, OnStarted());
  screen_publisher_->Start();

  EXPECT_EQ(instances, mdns_responder_factory_->instances());
  ASSERT_TRUE(mdns_responder->running());
  {
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(1u, interfaces.size());
    EXPECT_EQ(second_socket_, interfaces[0].socket);
  }

  EXPECT_CALL(observer_, OnStopped());
  screen_listener_->Stop();
  ASSERT_TRUE(mdns_responder->running());
  EXPECT_EQ(1u, mdns_responder->registered_interfaces().size());

  EXPECT_CALL(publisher_observer_, OnStopped());
  screen_publisher_->Stop();
  // TODO(btolsch): This is a use-after-free (here and in other tests).  Maybe
  // hook FakeMdnsResponderAdapter into the factory so it can report destruction
  // instead?  This could also disambiguate between suspended and stopped
  // transitions.
  EXPECT_FALSE(mdns_responder->running());
  EXPECT_EQ(0u, mdns_responder->registered_interfaces().size());
}

TEST_F(MdnsResponderServiceTest, PublishAndListen) {
  mdns_service_->SetServiceConfig(
      kTestHostname, kTestServiceInstance, kTestPort, {2},
      {std::string("fn=") + std::string(kTestFriendlyName)});

  EXPECT_CALL(publisher_observer_, OnStarted());
  screen_publisher_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();
  ASSERT_TRUE(mdns_responder);
  ASSERT_TRUE(mdns_responder->running());
  {
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(1u, interfaces.size());
    EXPECT_EQ(second_socket_, interfaces[0].socket);
  }

  auto instances = mdns_responder_factory_->instances();
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  EXPECT_EQ(instances, mdns_responder_factory_->instances());
  ASSERT_TRUE(mdns_responder->running());
  {
    auto interfaces = mdns_responder->registered_interfaces();
    ASSERT_EQ(1u, interfaces.size());
    EXPECT_EQ(second_socket_, interfaces[0].socket);
  }

  EXPECT_CALL(publisher_observer_, OnStopped());
  screen_publisher_->Stop();
  ASSERT_TRUE(mdns_responder->running());
  EXPECT_EQ(1u, mdns_responder->registered_interfaces().size());

  EXPECT_CALL(observer_, OnStopped());
  screen_listener_->Stop();
  EXPECT_FALSE(mdns_responder->running());
  EXPECT_EQ(0u, mdns_responder->registered_interfaces().size());
}

TEST_F(MdnsResponderServiceTest, AddressQueryStopped) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_));
  mdns_service_->HandleNewEvents({});

  auto srv_remove =
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "gigliorononomicon", 12345, default_socket_);
  srv_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddSrvEvent(std::move(srv_remove));

  EXPECT_CALL(observer_, OnScreenRemoved(::testing::_));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_FALSE(mdns_responder->srv_queries().empty());
  EXPECT_FALSE(mdns_responder->txt_queries().empty());
  EXPECT_TRUE(mdns_responder->a_queries().empty());
  EXPECT_TRUE(mdns_responder->aaaa_queries().empty());
}

TEST_F(MdnsResponderServiceTest, AddressQueryRefCount) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);
  AddEventsForNewService(mdns_responder, "instance-2", kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 4321,
                         {"fn=shwofty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_)).Times(2);
  mdns_service_->HandleNewEvents({});

  auto srv_remove =
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "gigliorononomicon", 12345, default_socket_);
  srv_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddSrvEvent(std::move(srv_remove));

  EXPECT_CALL(observer_, OnScreenRemoved(::testing::_));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_FALSE(mdns_responder->srv_queries().empty());
  EXPECT_FALSE(mdns_responder->txt_queries().empty());
  EXPECT_FALSE(mdns_responder->a_queries().empty());
  EXPECT_FALSE(mdns_responder->aaaa_queries().empty());

  srv_remove =
      MakeSrvEvent("instance-2", kTestServiceName, kTestServiceProtocol,
                   "gigliorononomicon", 4321, default_socket_);
  srv_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddSrvEvent(std::move(srv_remove));

  EXPECT_CALL(observer_, OnScreenRemoved(::testing::_));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_FALSE(mdns_responder->srv_queries().empty());
  EXPECT_FALSE(mdns_responder->txt_queries().empty());
  EXPECT_TRUE(mdns_responder->a_queries().empty());
  EXPECT_TRUE(mdns_responder->aaaa_queries().empty());
}

TEST_F(MdnsResponderServiceTest, ServiceQueriesStoppedSrvFirst) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_));
  mdns_service_->HandleNewEvents({});

  auto srv_remove =
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "gigliorononomicon", 12345, default_socket_);
  srv_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddSrvEvent(std::move(srv_remove));

  EXPECT_CALL(observer_, OnScreenRemoved(::testing::_));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_FALSE(mdns_responder->srv_queries().empty());
  EXPECT_FALSE(mdns_responder->txt_queries().empty());
  EXPECT_TRUE(mdns_responder->a_queries().empty());
  EXPECT_TRUE(mdns_responder->aaaa_queries().empty());

  auto ptr_remove = MakePtrEvent(kTestServiceInstance, kTestServiceName,
                                 kTestServiceProtocol, default_socket_);
  ptr_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddPtrEvent(std::move(ptr_remove));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_TRUE(mdns_responder->srv_queries().empty());
  EXPECT_TRUE(mdns_responder->txt_queries().empty());
  EXPECT_TRUE(mdns_responder->a_queries().empty());
  EXPECT_TRUE(mdns_responder->aaaa_queries().empty());
}

TEST_F(MdnsResponderServiceTest, ServiceQueriesStoppedPtrFirst) {
  EXPECT_CALL(observer_, OnStarted());
  screen_listener_->Start();

  auto* mdns_responder = mdns_responder_factory_->last_mdns_responder();

  AddEventsForNewService(mdns_responder, kTestServiceInstance, kTestServiceName,
                         kTestServiceProtocol, "gigliorononomicon", 12345,
                         {"fn=shifty", "id=asdf"}, IPAddress{192, 168, 3, 7},
                         default_socket_);

  EXPECT_CALL(observer_, OnScreenAdded(::testing::_));
  mdns_service_->HandleNewEvents({});

  auto ptr_remove = MakePtrEvent(kTestServiceInstance, kTestServiceName,
                                 kTestServiceProtocol, default_socket_);
  ptr_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddPtrEvent(std::move(ptr_remove));

  EXPECT_CALL(observer_, OnScreenRemoved(::testing::_));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_FALSE(mdns_responder->srv_queries().empty());
  EXPECT_FALSE(mdns_responder->txt_queries().empty());
  EXPECT_FALSE(mdns_responder->a_queries().empty());
  EXPECT_FALSE(mdns_responder->aaaa_queries().empty());

  auto srv_remove =
      MakeSrvEvent(kTestServiceInstance, kTestServiceName, kTestServiceProtocol,
                   "gigliorononomicon", 12345, default_socket_);
  srv_remove.header.response_type = mdns::QueryEventHeader::Type::kRemoved;
  mdns_responder->AddSrvEvent(std::move(srv_remove));
  mdns_service_->HandleNewEvents({});

  EXPECT_FALSE(mdns_responder->ptr_queries().empty());
  EXPECT_TRUE(mdns_responder->srv_queries().empty());
  EXPECT_TRUE(mdns_responder->txt_queries().empty());
  EXPECT_TRUE(mdns_responder->a_queries().empty());
  EXPECT_TRUE(mdns_responder->aaaa_queries().empty());
}

}  // namespace openscreen
