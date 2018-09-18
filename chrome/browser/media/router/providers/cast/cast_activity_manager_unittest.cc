// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/common/buffered_message_sender.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::IsEmpty;
using testing::Not;

namespace media_router {

namespace {
constexpr char kOrigin[] = "https://google.com";
constexpr int kTabId = 1;
}  // namespace

class ClientPresentationConnection
    : public blink::mojom::PresentationConnection {
 public:
  explicit ClientPresentationConnection(
      mojom::RoutePresentationConnectionPtr connections)
      : binding_(this, std::move(connections->connection_request)),
        connection_(std::move(connections->connection_ptr)) {}

  ~ClientPresentationConnection() override = default;

  void SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessagePtr message) {
    connection_->OnMessage(std::move(message));
  }

  MOCK_METHOD1(OnMessage, void(blink::mojom::PresentationConnectionMessagePtr));
  MOCK_METHOD1(DidChangeState,
               void(blink::mojom::PresentationConnectionState state));
  MOCK_METHOD0(RequestClose, void());

  mojo::Binding<blink::mojom::PresentationConnection> binding_;
  blink::mojom::PresentationConnectionPtr connection_;
  DISALLOW_COPY_AND_ASSIGN(ClientPresentationConnection);
};

class CastActivityManagerTest : public testing::Test {
 public:
  CastActivityManagerTest()
      : connector_factory_(
            service_manager::TestConnectorFactory::CreateForUniqueService(
                std::make_unique<data_decoder::DataDecoderService>())),
        connector_(connector_factory_->CreateConnector()),
        socket_service_(base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI})),
        message_handler_(&socket_service_) {
    media_sink_service_.AddOrUpdateSink(sink_);
    socket_.set_id(sink_.cast_data().cast_channel_id);
  }

  ~CastActivityManagerTest() override = default;

  void SetUp() override {
    router_binding_ = std::make_unique<mojo::Binding<mojom::MediaRouter>>(
        &mock_router_, mojo::MakeRequest(&router_ptr_));

    manager_ = std::make_unique<CastActivityManager>(
        &media_sink_service_, &message_handler_, router_ptr_.get(),
        std::make_unique<DataDecoder>(connector_.get()), "hash-token");

    // Make sure we get route updates.
    manager_->AddRouteQuery(MediaSource::Id());
  }

  void TearDown() override { manager_.reset(); }

  void VerifyAndClearExpectations() {
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&message_handler_));
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_router_));
  }

  void ExpectLaunchSessionSuccess(
      const base::Optional<MediaRoute>& route,
      mojom::RoutePresentationConnectionPtr presentation_connections,
      const base::Optional<std::string>&,
      media_router::RouteRequestResult::ResultCode) {
    ASSERT_TRUE(route);
    route_ = std::make_unique<MediaRoute>(*route);
    client_connection_ = std::make_unique<ClientPresentationConnection>(
        std::move(presentation_connections));
    // When client is connected, the receiver_action message will be sent.
    EXPECT_CALL(
        *client_connection_,
        DidChangeState(blink::mojom::PresentationConnectionState::CONNECTED));
    EXPECT_CALL(*client_connection_, OnMessage(_));
  }

  void LaunchSession() {
    auto source = CastMediaSource::From("cast:ABCDEFGH?clientId=12345");
    ASSERT_TRUE(source);

    // MediaRouter is notified of new route.
    EXPECT_CALL(mock_router_, OnRoutesUpdated(MediaRouteProviderId::CAST,
                                              Not(IsEmpty()), _, _));

    // A launch session request is sent to the sink.
    EXPECT_CALL(message_handler_,
                LaunchSession(sink_.cast_data().cast_channel_id, "ABCDEFGH",
                              kDefaultLaunchTimeout, _))
        .WillOnce([this](int, const std::string&, base::TimeDelta,
                         cast_channel::LaunchSessionCallback callback) {
          launch_session_callback_ = std::move(callback);
        });

    // Callback will be invoked synchronously.
    manager_->LaunchSession(
        *source, sink_, "presentationId", url::Origin::Create(GURL(kOrigin)),
        kTabId,
        /*incognito*/ false,
        base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                       base::Unretained(this)));
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();
  }

  cast_channel::LaunchSessionResponse GetSuccessLaunchResponse() {
    std::string receiver_status_str = R"({
        "applications": [{
          "appId": "ABCDEFGH",
          "displayName": "App display name",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "sessionId",
          "statusText":"App status",
          "transportId":"transportId"
        }]
    })";
    auto receiver_status = base::JSONReader::Read(receiver_status_str);
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kOk;
    response.receiver_status = std::move(*receiver_status);
    return response;
  }

  // Precondition: |LaunchSession()| must be called first.
  void LaunchSessionResponseSuccess() {
    // 3 things will happen:
    // (1) SDK client receives new_session message.
    // (2) Virtual connection is created.
    // (3) Route list will be updated.
    EXPECT_CALL(message_handler_,
                EnsureConnection(sink_.cast_data().cast_channel_id, "12345",
                                 "transportId"));
    std::move(launch_session_callback_).Run(GetSuccessLaunchResponse());

    EXPECT_CALL(*client_connection_, OnMessage(_));
    EXPECT_CALL(mock_router_, OnRoutesUpdated(MediaRouteProviderId::CAST,
                                              Not(IsEmpty()), _, _));
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();
  }

  // Precondition: |LaunchSession()| must be called first.
  void LaunchSessionResponseFailure() {
    // 2 things will happen:
    // (1) Route is removed
    // (2) Issue will be sent.
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kError;
    std::move(launch_session_callback_).Run(std::move(response));

    EXPECT_CALL(mock_router_, OnIssue(_));
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, IsEmpty(), _, _));
    EXPECT_CALL(
        *client_connection_,
        DidChangeState(blink::mojom::PresentationConnectionState::TERMINATED));
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();
  }

  // Precondition: |LaunchSession()| must be called first.
  void TerminateSession(bool success) {
    cast_channel::StopSessionCallback stop_session_callback;

    EXPECT_CALL(message_handler_,
                StopSession(sink_.cast_data().cast_channel_id, "sessionId", _))
        .WillOnce([&stop_session_callback](
                      int, const std::string&,
                      cast_channel::StopSessionCallback callback) {
          stop_session_callback = std::move(callback);
        });
    manager_->TerminateSession(
        route_->media_route_id(),
        base::BindOnce(
            success ? &CastActivityManagerTest::ExpectTerminateResultSuccess
                    : &CastActivityManagerTest::ExpectTerminateResultFailure,
            base::Unretained(this)));
    // Receiver action stop message is sent to SDK client.
    EXPECT_CALL(*client_connection_, OnMessage(_));
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();

    std::move(stop_session_callback).Run(success);
  }

  // Precondition: |LaunchSession()| called, |LaunchSessionResponseSuccess()|
  // not called.
  void TerminateNoSession() {
    // Stop session message not sent because session has not launched yet.
    EXPECT_CALL(message_handler_, StopSession(_, _, _)).Times(0);
    manager_->TerminateSession(
        route_->media_route_id(),
        base::BindOnce(&CastActivityManagerTest::ExpectTerminateResultSuccess,
                       base::Unretained(this)));
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();
  }

  void ExpectTerminateResultSuccess(
      const base::Optional<std::string>& error_text,
      RouteRequestResult::ResultCode result_code) {
    EXPECT_EQ(RouteRequestResult::OK, result_code);
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, IsEmpty(), _, _));
    EXPECT_CALL(
        *client_connection_,
        DidChangeState(blink::mojom::PresentationConnectionState::TERMINATED));
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();
  }

  void ExpectTerminateResultFailure(
      const base::Optional<std::string>& error_text,
      RouteRequestResult::ResultCode result_code) {
    EXPECT_NE(RouteRequestResult::OK, result_code);
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, _, _, _))
        .Times(0);
    thread_bundle_.RunUntilIdle();
    VerifyAndClearExpectations();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  MockMojoMediaRouter mock_router_;
  mojom::MediaRouterPtr router_ptr_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouter>> router_binding_;

  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastSocket socket_;
  cast_channel::MockCastMessageHandler message_handler_;

  MediaSinkInternal sink_ = CreateCastSink(1);
  std::unique_ptr<MediaRoute> route_;
  std::unique_ptr<ClientPresentationConnection> client_connection_;
  cast_channel::LaunchSessionCallback launch_session_callback_;

  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastActivityManager> manager_;
};

TEST_F(CastActivityManagerTest, LaunchSession) {
  LaunchSession();
  LaunchSessionResponseSuccess();
}

TEST_F(CastActivityManagerTest, LaunchSessionFails) {
  LaunchSession();
  LaunchSessionResponseFailure();
}

TEST_F(CastActivityManagerTest, TerminateSession) {
  LaunchSession();
  LaunchSessionResponseSuccess();
  TerminateSession(true);
}

TEST_F(CastActivityManagerTest, TerminateSessionFails) {
  LaunchSession();
  LaunchSessionResponseSuccess();
  TerminateSession(false);
}

TEST_F(CastActivityManagerTest, TerminateSessionBeforeLaunchResponse) {
  LaunchSession();
  TerminateNoSession();

  // Route already terminated, so no-op when handling launch response.
  std::move(launch_session_callback_).Run(GetSuccessLaunchResponse());
  EXPECT_CALL(mock_router_,
              OnRoutesUpdated(MediaRouteProviderId::CAST, _, _, _))
      .Times(0);
  thread_bundle_.RunUntilIdle();
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiver) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Destination ID matches client ID.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "12345");
  message_handler_.OnMessage(socket_, message);
  EXPECT_CALL(*client_connection_, OnMessage(_));
  thread_bundle_.RunUntilIdle();
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiverAllDestinations) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Matches all destinations.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "*");
  message_handler_.OnMessage(socket_, message);
  EXPECT_CALL(*client_connection_, OnMessage(_));
  thread_bundle_.RunUntilIdle();
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiverUnknownDestination) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Destination ID does not match client ID.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "99999");
  message_handler_.OnMessage(socket_, message);
  EXPECT_CALL(*client_connection_, OnMessage(_)).Times(0);
  thread_bundle_.RunUntilIdle();
}

TEST_F(CastActivityManagerTest, AppMessageFromClient) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  EXPECT_CALL(message_handler_,
              SendAppMessage(sink_.cast_data().cast_channel_id, _));
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(
          R"({
        "type": "app_message",
        "clientId": "12345",
        "message": {
          "namespaceName": "urn:x-cast:com.google.foo",
          "sessionId": "sessionId",
          "message": {}
        }
      })"));

  // An ACK message is sent back to client.
  EXPECT_CALL(*client_connection_, OnMessage(_));
  thread_bundle_.RunUntilIdle();
}

TEST_F(CastActivityManagerTest, AppMessageFromClientInvalidNamespace) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Message namespace not in set of allowed namespaces.
  EXPECT_CALL(message_handler_,
              SendAppMessage(sink_.cast_data().cast_channel_id, _))
      .Times(0);
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(
          R"({
        "type": "app_message",
        "clientId": "12345",
        "message": {
          "namespaceName": "someOtherNamespace",
          "sessionId": "sessionId",
          "message": {}
        }
      })"));
}

}  // namespace media_router
