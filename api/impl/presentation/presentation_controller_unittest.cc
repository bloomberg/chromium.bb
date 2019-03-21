// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_controller.h"

#include <string>
#include <vector>

#include "api/impl/quic/testing/quic_test_support.h"
#include "api/impl/service_listener_impl.h"
#include "api/impl/testing/fake_clock.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/testing/message_demuxer_test_support.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace presentation {

using ::testing::_;
using ::testing::Invoke;

constexpr char kTestUrl[] = "https://example.foo";

class MockServiceListenerDelegate final : public ServiceListenerImpl::Delegate {
 public:
  ~MockServiceListenerDelegate() override = default;

  ServiceListenerImpl* listener() { return listener_; }

  MOCK_METHOD0(StartListener, void());
  MOCK_METHOD0(StartAndSuspendListener, void());
  MOCK_METHOD0(StopListener, void());
  MOCK_METHOD0(SuspendListener, void());
  MOCK_METHOD0(ResumeListener, void());
  MOCK_METHOD1(SearchNow, void(ServiceListener::State from));
};

class MockReceiverObserver final : public ReceiverObserver {
 public:
  ~MockReceiverObserver() override = default;

  MOCK_METHOD2(OnRequestFailed,
               void(const std::string& presentation_url,
                    const std::string& service_id));
  MOCK_METHOD2(OnReceiverAvailable,
               void(const std::string& presentation_url,
                    const std::string& service_id));
  MOCK_METHOD2(OnReceiverUnavailable,
               void(const std::string& presentation_url,
                    const std::string& service_id));
};

class ControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto service_listener =
        std::make_unique<ServiceListenerImpl>(&mock_listener_delegate_);
    NetworkServiceManager::Create(std::move(service_listener), nullptr,
                                  std::move(quic_bridge_.quic_client),
                                  std::move(quic_bridge_.quic_server));
    controller_ = std::make_unique<Controller>(&fake_clock_);
    ON_CALL(quic_bridge_.mock_server_observer, OnIncomingConnectionMock(_))
        .WillByDefault(
            Invoke([this](std::unique_ptr<ProtocolConnection>& connection) {
              controller_endpoint_id_ = connection->endpoint_id();
            }));

    availability_watch_ =
        quic_bridge_.receiver_demuxer->SetDefaultMessageTypeWatch(
            msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback_);
  }

  void TearDown() override {
    availability_watch_ = MessageDemuxer::MessageWatch();
    controller_.reset();
    NetworkServiceManager::Dispose();
  }

  void ExpectAvailabilityRequest(
      MockMessageCallback* mock_callback,
      msgs::PresentationUrlAvailabilityRequest* request) {
    EXPECT_CALL(*mock_callback, OnStreamMessage(_, _, _, _, _, _))
        .WillOnce(
            Invoke([request](uint64_t endpoint_id, uint64_t cid,
                             msgs::Type message_type, const uint8_t* buffer,
                             size_t buffer_size, platform::TimeDelta now) {
              ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                  buffer, buffer_size, request);
              return result;
            }));
  }

  void SendAvailabilityResponse(
      const msgs::PresentationUrlAvailabilityResponse& response) {
    std::unique_ptr<ProtocolConnection> controller_connection =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionServer()
            ->CreateProtocolConnection(controller_endpoint_id_);
    ASSERT_TRUE(controller_connection);
    ASSERT_EQ(Error::Code::kNone,
              controller_connection
                  ->WriteMessage(
                      response, msgs::EncodePresentationUrlAvailabilityResponse)
                  .code());
  }

  void SendAvailabilityEvent(
      const msgs::PresentationUrlAvailabilityEvent& event) {
    std::unique_ptr<ProtocolConnection> controller_connection =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionServer()
            ->CreateProtocolConnection(controller_endpoint_id_);
    ASSERT_TRUE(controller_connection);
    ASSERT_EQ(
        Error::Code::kNone,
        controller_connection
            ->WriteMessage(event, msgs::EncodePresentationUrlAvailabilityEvent)
            .code());
  }

  MessageDemuxer::MessageWatch availability_watch_;
  MockMessageCallback mock_callback_;
  FakeClock fake_clock_{platform::TimeDelta::FromSeconds(11111)};
  MockServiceListenerDelegate mock_listener_delegate_;
  FakeQuicBridge quic_bridge_;
  std::unique_ptr<Controller> controller_;
  ServiceInfo receiver_info1{"service-id1",
                             "lucas-auer",
                             1,
                             quic_bridge_.kReceiverEndpoint,
                             {}};
  MockReceiverObserver mock_receiver_observer_;
  uint64_t controller_endpoint_id_{0};
};

TEST_F(ControllerTest, ReceiverWatchMoves) {
  std::vector<std::string> urls{"one fish", "two fish", "red fish", "gnu fish"};
  MockReceiverObserver mock_observer;

  Controller::ReceiverWatch watch1(controller_.get(), urls, &mock_observer);
  EXPECT_TRUE(watch1);
  Controller::ReceiverWatch watch2;
  EXPECT_FALSE(watch2);
  watch2 = std::move(watch1);
  EXPECT_FALSE(watch1);
  EXPECT_TRUE(watch2);
  Controller::ReceiverWatch watch3(std::move(watch2));
  EXPECT_FALSE(watch2);
  EXPECT_TRUE(watch3);
}

TEST_F(ControllerTest, ConnectRequestMoves) {
  std::string service_id{"service-id1"};
  uint64_t request_id = 7;

  Controller::ConnectRequest request1(controller_.get(), service_id, false,
                                      request_id);
  EXPECT_TRUE(request1);
  Controller::ConnectRequest request2;
  EXPECT_FALSE(request2);
  request2 = std::move(request1);
  EXPECT_FALSE(request1);
  EXPECT_TRUE(request2);
  Controller::ConnectRequest request3(std::move(request2));
  EXPECT_FALSE(request2);
  EXPECT_TRUE(request3);
}

TEST_F(ControllerTest, ReceiverAvailable) {
  mock_listener_delegate_.listener()->OnReceiverAdded(receiver_info1);
  Controller::ReceiverWatch watch =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer_);

  msgs::PresentationUrlAvailabilityRequest request;
  ExpectAvailabilityRequest(&mock_callback_, &request);
  quic_bridge_.RunTasksUntilIdle();

  msgs::PresentationUrlAvailabilityResponse response;
  response.request_id = request.request_id;
  response.url_availabilities.push_back(msgs::kCompatible);
  SendAvailabilityResponse(response);
  EXPECT_CALL(mock_receiver_observer_, OnReceiverAvailable(_, _));
  quic_bridge_.RunTasksUntilIdle();

  MockReceiverObserver mock_receiver_observer2;
  EXPECT_CALL(mock_receiver_observer2, OnReceiverAvailable(_, _));
  Controller::ReceiverWatch watch2 =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer2);
}

TEST_F(ControllerTest, ReceiverWatchCancel) {
  mock_listener_delegate_.listener()->OnReceiverAdded(receiver_info1);
  Controller::ReceiverWatch watch =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer_);

  msgs::PresentationUrlAvailabilityRequest request;
  ExpectAvailabilityRequest(&mock_callback_, &request);
  quic_bridge_.RunTasksUntilIdle();

  msgs::PresentationUrlAvailabilityResponse response;
  response.request_id = request.request_id;
  response.url_availabilities.push_back(msgs::kCompatible);
  SendAvailabilityResponse(response);
  EXPECT_CALL(mock_receiver_observer_, OnReceiverAvailable(_, _));
  quic_bridge_.RunTasksUntilIdle();

  MockReceiverObserver mock_receiver_observer2;
  EXPECT_CALL(mock_receiver_observer2, OnReceiverAvailable(_, _));
  Controller::ReceiverWatch watch2 =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer2);

  watch = Controller::ReceiverWatch();
  msgs::PresentationUrlAvailabilityEvent event;
  event.watch_id = request.watch_id;
  event.urls.emplace_back(kTestUrl);
  event.url_availabilities.push_back(msgs::kNotCompatible);

  EXPECT_CALL(mock_receiver_observer2, OnReceiverUnavailable(_, _));
  EXPECT_CALL(mock_receiver_observer_, OnReceiverUnavailable(_, _)).Times(0);
  SendAvailabilityEvent(event);
  quic_bridge_.RunTasksUntilIdle();
}

}  // namespace presentation
}  // namespace openscreen
