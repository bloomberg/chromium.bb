// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/common/buffered_message_sender.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJsonDeprecated;
using testing::_;
using testing::AnyNumber;
using testing::IsEmpty;
using testing::Not;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {
constexpr int kChannelId = 42;
constexpr char kOrigin[] = "https://google.com";
constexpr int kTabId = 1;
constexpr char kSource1[] = "cast:ABCDEFGH?clientId=theClientId";
constexpr char kSource2[] = "cast:BBBBBBBB?clientId=theClientId";
constexpr char kReceiverStatus[] = R"({
        "applications": [{
          "appId": "ABCDEFGH",
          "displayName": "App display name",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId",
          "statusText":"App status",
          "transportId":"theTransportId"
        }]
      })";
constexpr char kReceiverStatus2[] = R"({
        "applications": [{
          "appId": "ABCDEFGH",
          "displayName": "Updated display name",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId",
          "statusText":"App status",
          "transportId":"theTransportId"
        }]
      })";
constexpr char kReceiverStatus3[] = R"({
        "applications": [{
          "appId": "BBBBBBBB",
          "displayName": "Another app",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId2",
          "statusText":"App status",
          "transportId":"theTransportId"
        }]
      })";
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
  MOCK_METHOD1(DidClose, void(blink::mojom::PresentationConnectionCloseReason));

  mojo::Binding<blink::mojom::PresentationConnection> binding_;
  blink::mojom::PresentationConnectionPtr connection_;
  DISALLOW_COPY_AND_ASSIGN(ClientPresentationConnection);
};

// Test parameters are a boolean indicating whether the client connection should
// be closed by a leave_session message, and the URL used to create the test
// session.
class CastActivityManagerTest
    : public testing::TestWithParam<std::pair<bool, const char*>> {
 public:
  CastActivityManagerTest()
      : data_decoder_service_(connector_factory_.RegisterInstance(
            data_decoder::mojom::kServiceName)),
        socket_service_(base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI})),
        message_handler_(&socket_service_) {
    media_sink_service_.AddOrUpdateSink(sink_);
    socket_.set_id(kChannelId);
  }

  ~CastActivityManagerTest() override = default;

  void SetUp() override {
    router_binding_ = std::make_unique<mojo::Binding<mojom::MediaRouter>>(
        &mock_router_, mojo::MakeRequest(&router_ptr_));

    delete session_tracker_;
    session_tracker_ = new CastSessionTracker(
        &media_sink_service_, &message_handler_, socket_service_.task_runner());
    manager_ = std::make_unique<CastActivityManager>(
        &media_sink_service_, session_tracker_, &message_handler_,
        router_ptr_.get(),
        std::make_unique<DataDecoder>(connector_factory_.GetDefaultConnector()),
        "hash-token");

    RunUntilIdle();

    // Make sure we get route updates.
    manager_->AddRouteQuery(MediaSource::Id());
  }

  void TearDown() override {
    // This is a no-op for many tests, but it serves as a good sanity check in
    // any case.
    RunUntilIdle();

    manager_.reset();
  }

  // Run any pending events and verify expectations associated with them.  This
  // method is sometimes called when there are clearly no pending events simply
  // to check expectations for code executed synchronously.
  void RunUntilIdle() {
    thread_bundle_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&message_handler_);
    testing::Mock::VerifyAndClearExpectations(&mock_router_);
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
    EXPECT_CALL(*client_connection_, OnMessage).RetiresOnSaturation();
  }

  void LaunchSession(const char* sourceId = kSource1) {
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();

    // A launch session request is sent to the sink.
    EXPECT_CALL(message_handler_,
                LaunchSession(kChannelId, "ABCDEFGH", kDefaultLaunchTimeout, _))
        .WillOnce(WithArg<3>([this](auto callback) {
          launch_session_callback_ = std::move(callback);
        }));

    auto source = CastMediaSource::FromMediaSourceId(sourceId);
    ASSERT_TRUE(source);

    // Callback will be invoked synchronously.
    manager_->LaunchSession(
        *source, sink_, "presentationId", origin_, kTabId,
        /*incognito*/ false,
        base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                       base::Unretained(this)));

    RunUntilIdle();
  }

  cast_channel::LaunchSessionResponse GetSuccessLaunchResponse() {
    auto receiver_status = ParseJsonDeprecated(kReceiverStatus);
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
                EnsureConnection(kChannelId, "theClientId", "theTransportId"));

    auto response = GetSuccessLaunchResponse();
    session_tracker_->SetSessionForTest(
        route_->media_sink_id(),
        CastSession::From(sink_, *response.receiver_status));
    std::move(launch_session_callback_).Run(std::move(response));
    EXPECT_CALL(*client_connection_, OnMessage).RetiresOnSaturation();
    ExpectSingleRouteUpdate();
    RunUntilIdle();
  }

  // Precondition: |LaunchSession()| must be called first.
  void LaunchSessionResponseFailure() {
    // 2 things will happen:
    // (1) Route is removed
    // (2) Issue will be sent.
    cast_channel::LaunchSessionResponse response;
    response.result = cast_channel::LaunchSessionResponse::Result::kError;
    std::move(launch_session_callback_).Run(std::move(response));

    EXPECT_CALL(mock_router_, OnIssue);
    ExpectEmptyRouteUpdate();
    EXPECT_CALL(
        *client_connection_,
        DidChangeState(blink::mojom::PresentationConnectionState::TERMINATED));
    RunUntilIdle();
  }

  // Precondition: |LaunchSession()| must be called first.
  void TerminateSession(cast_channel::Result result) {
    cast_channel::ResultCallback stop_session_callback;

    EXPECT_CALL(message_handler_, StopSession(kChannelId, "theSessionId",
                                              base::Optional<std::string>(), _))
        .WillOnce(WithArg<3>([&](auto callback) {
          stop_session_callback = std::move(callback);
        }));
    manager_->TerminateSession(
        route_->media_route_id(),
        base::BindOnce(
            result == cast_channel::Result::kOk
                ? &CastActivityManagerTest::ExpectTerminateResultSuccess
                : &CastActivityManagerTest::ExpectTerminateResultFailure,
            base::Unretained(this)));
    // Receiver action stop message is sent to SDK client.
    EXPECT_CALL(*client_connection_, OnMessage);
    RunUntilIdle();

    std::move(stop_session_callback).Run(result);
  }

  // Precondition: |LaunchSession()| called, |LaunchSessionResponseSuccess()|
  // not called.
  void TerminateNoSession() {
    // Stop session message not sent because session has not launched yet.
    EXPECT_CALL(message_handler_, StopSession).Times(0);
    manager_->TerminateSession(
        route_->media_route_id(),
        base::BindOnce(&CastActivityManagerTest::ExpectTerminateResultSuccess,
                       base::Unretained(this)));
    RunUntilIdle();
  }

  void ExpectTerminateResultSuccess(
      const base::Optional<std::string>& error_text,
      RouteRequestResult::ResultCode result_code) {
    EXPECT_EQ(RouteRequestResult::OK, result_code);
    ExpectEmptyRouteUpdate();
    EXPECT_CALL(
        *client_connection_,
        DidChangeState(blink::mojom::PresentationConnectionState::TERMINATED));
    RunUntilIdle();
  }

  void ExpectTerminateResultFailure(
      const base::Optional<std::string>& error_text,
      RouteRequestResult::ResultCode result_code) {
    EXPECT_NE(RouteRequestResult::OK, result_code);
    ExpectNoRouteUpdate();
    RunUntilIdle();
  }

  // Expect a call to OnRoutesUpdated() with a single route, which will
  // optionally be saved in the variable pointed to by |route_ptr|.
  void ExpectSingleRouteUpdate(MediaRoute* route_ptr = nullptr) {
    EXPECT_CALL(mock_router_, OnRoutesUpdated(MediaRouteProviderId::CAST,
                                              Not(IsEmpty()), _, _))
        .WillOnce(WithArg<1>([=](auto routes) {
          EXPECT_EQ(1u, routes.size());
          if (route_ptr) {
            *route_ptr = routes[0];
          }
        }));
  }

  // Expect a call to OnRoutesUpdated() with no routes.
  void ExpectEmptyRouteUpdate() {
    EXPECT_CALL(mock_router_,
                OnRoutesUpdated(MediaRouteProviderId::CAST, IsEmpty(), _, _))
        .Times(1);
  }

  // Expect that OnRoutesUpdated() will not be called.
  void ExpectNoRouteUpdate() {
    EXPECT_CALL(mock_router_, OnRoutesUpdated).Times(0);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride parser_override_;
  service_manager::TestConnectorFactory connector_factory_;
  data_decoder::DataDecoderService data_decoder_service_;

  MockMojoMediaRouter mock_router_;
  mojom::MediaRouterPtr router_ptr_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouter>> router_binding_;

  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastSocket socket_;
  cast_channel::MockCastMessageHandler message_handler_;

  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  std::unique_ptr<MediaRoute> route_;
  std::unique_ptr<ClientPresentationConnection> client_connection_;
  cast_channel::LaunchSessionCallback launch_session_callback_;

  TestMediaSinkService media_sink_service_;
  MockCastAppDiscoveryService app_discovery_service_;
  std::unique_ptr<CastActivityManager> manager_;

  // We mus use a raw pointer instead of a smart pointer because
  // CastSessionTracker's constructor and destructor are private.
  CastSessionTracker* session_tracker_ = nullptr;

  const url::Origin origin_ = url::Origin::Create(GURL(kOrigin));
};

TEST_F(CastActivityManagerTest, LaunchSession) {
  LaunchSession();
  LaunchSessionResponseSuccess();
}

TEST_F(CastActivityManagerTest, LaunchSessionFails) {
  LaunchSession();
  LaunchSessionResponseFailure();
}

TEST_F(CastActivityManagerTest, LaunchSessionTerminatesExistingSessionOnSink) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Receiver action stop message is sent to SDK client.
  EXPECT_CALL(*client_connection_, OnMessage);

  // Existing session will be terminated.
  cast_channel::ResultCallback stop_session_callback;
  EXPECT_CALL(message_handler_, StopSession(kChannelId, "theSessionId",
                                            base::Optional<std::string>(), _))
      .WillOnce(WithArg<3>(
          [&](auto callback) { stop_session_callback = std::move(callback); }));

  // Launch a new session on the same sink.
  auto source = CastMediaSource::FromMediaSourceId(kSource2);
  ASSERT_TRUE(source);
  manager_->LaunchSession(
      *source, sink_, "presentationId2", origin_, kTabId, /*incognito*/
      false,
      base::BindOnce(&CastActivityManagerTest::ExpectLaunchSessionSuccess,
                     base::Unretained(this)));
  RunUntilIdle();

  {
    testing::InSequence dummy;

    // Existing route is terminated before new route is created.
    // MediaRouter is notified of terminated route.
    ExpectEmptyRouteUpdate();

    // After existing route is terminated, new route is created.
    // MediaRouter is notified of new route.
    ExpectSingleRouteUpdate();
  }

  // A launch session request is sent to the sink.
  EXPECT_CALL(message_handler_,
              LaunchSession(kChannelId, "BBBBBBBB", kDefaultLaunchTimeout, _));

  std::move(stop_session_callback).Run(cast_channel::Result::kOk);
}

TEST_F(CastActivityManagerTest, AddRemoveNonLocalActivity) {
  auto session =
      CastSession::From(sink_, *ParseJsonDeprecated(kReceiverStatus));
  ASSERT_TRUE(session);

  MediaRoute route;
  ExpectSingleRouteUpdate(&route);
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();

  EXPECT_FALSE(route.is_local());

  ExpectEmptyRouteUpdate();
  manager_->OnSessionRemoved(sink_);
}

TEST_F(CastActivityManagerTest, UpdateNewlyCreatedSession) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  auto session =
      CastSession::From(sink_, *ParseJsonDeprecated(kReceiverStatus));
  ASSERT_TRUE(session);

  MediaRoute route;
  ExpectSingleRouteUpdate(&route);
  EXPECT_CALL(*client_connection_, OnMessage);

  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();

  EXPECT_TRUE(route.is_local());
  EXPECT_EQ(route.description(), session->GetRouteDescription());
}

TEST_F(CastActivityManagerTest, UpdateExistingSession) {
  // Create and add the session to be updated, and verify it was added.
  auto session =
      CastSession::From(sink_, *ParseJsonDeprecated(kReceiverStatus));
  ASSERT_TRUE(session);
  MediaRoute route;
  ExpectSingleRouteUpdate(&route);
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  EXPECT_EQ(route.description(), session->GetRouteDescription());
  auto old_route_id = route.media_route_id();

  // Description change should be reflect in route update.
  auto updated_session =
      CastSession::From(sink_, *ParseJsonDeprecated(kReceiverStatus2));
  ASSERT_TRUE(updated_session);

  ExpectSingleRouteUpdate(&route);

  manager_->OnSessionAddedOrUpdated(sink_, *updated_session);
  RunUntilIdle();

  EXPECT_EQ(route.description(), updated_session->GetRouteDescription());
  EXPECT_EQ(old_route_id, route.media_route_id());
}

TEST_F(CastActivityManagerTest, ReplaceExistingSession) {
  // Create and add the session to be replaced, and verify it was added.
  auto session =
      CastSession::From(sink_, *ParseJsonDeprecated(kReceiverStatus));
  ASSERT_TRUE(session);
  MediaRoute route;
  ExpectSingleRouteUpdate(&route);
  manager_->OnSessionAddedOrUpdated(sink_, *session);
  RunUntilIdle();
  auto old_route_id = route.media_route_id();
  EXPECT_EQ(route.description(), session->GetRouteDescription());

  // Different session.
  auto new_session =
      CastSession::From(sink_, *ParseJsonDeprecated(kReceiverStatus3));
  ASSERT_TRUE(new_session);

  ExpectSingleRouteUpdate(&route);
  manager_->OnSessionAddedOrUpdated(sink_, *new_session);
  RunUntilIdle();

  EXPECT_EQ(route.description(), new_session->GetRouteDescription());
  EXPECT_NE(old_route_id, route.media_route_id());
}

TEST_F(CastActivityManagerTest, TerminateSession) {
  LaunchSession();
  LaunchSessionResponseSuccess();
  TerminateSession(cast_channel::Result::kOk);
}

TEST_F(CastActivityManagerTest, TerminateSessionFails) {
  LaunchSession();
  LaunchSessionResponseSuccess();
  TerminateSession(cast_channel::Result::kFailed);
}

TEST_F(CastActivityManagerTest, TerminateSessionBeforeLaunchResponse) {
  LaunchSession();
  TerminateNoSession();

  // Route already terminated, so no-op when handling launch response.
  std::move(launch_session_callback_).Run(GetSuccessLaunchResponse());
  ExpectNoRouteUpdate();
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiver) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Destination ID matches client ID.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "theClientId");
  message_handler_.OnMessage(socket_, message);
  EXPECT_CALL(*client_connection_, OnMessage);
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiverAllDestinations) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Matches all destinations.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "*");
  message_handler_.OnMessage(socket_, message);
  EXPECT_CALL(*client_connection_, OnMessage);
}

TEST_F(CastActivityManagerTest, AppMessageFromReceiverUnknownDestination) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Destination ID does not match client ID.
  cast_channel::CastMessage message = cast_channel::CreateCastMessage(
      "urn:x-cast:com.google.foo", base::Value(base::Value::Type::DICTIONARY),
      "sourceId", "99999");
  message_handler_.OnMessage(socket_, message);
  EXPECT_CALL(*client_connection_, OnMessage).Times(0);
}

TEST_F(CastActivityManagerTest, AppMessageFromClient) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  EXPECT_CALL(message_handler_, SendAppMessage(kChannelId, _))
      .WillOnce(Return(cast_channel::Result::kOk));
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "app_message",
        "clientId": "theClientId",
        "message": {
          "namespaceName": "urn:x-cast:com.google.foo",
          "sessionId": "theSessionId",
          "message": {}
        },
        "sequenceNumber": 123
      })"));

  // An ACK message is sent back to client.
  EXPECT_CALL(*client_connection_, OnMessage);
}

TEST_F(CastActivityManagerTest, AppMessageFromClientInvalidNamespace) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Message namespace not in set of allowed namespaces.
  EXPECT_CALL(message_handler_, SendAppMessage(kChannelId, _)).Times(0);
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "app_message",
        "clientId": "theClientId",
        "message": {
          "namespaceName": "someOtherNamespace",
          "sessionId": "theSessionId",
          "message": {}
        }
      })"));
}

TEST_F(CastActivityManagerTest, OnMediaStatusUpdated) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  EXPECT_CALL(*client_connection_, OnMessage(IsCastMessage(R"({
    "clientId": "theClientId",
    "message": {"foo": "bar"},
    "timeoutMillis": 0,
    "type": "v2_message"
  })")));
  manager_->OnMediaStatusUpdated(
      sink_, *ParseJsonDeprecated(R"({"foo": "bar"})"), 345);
}

TEST_F(CastActivityManagerTest, OnMediaStatusUpdatedWithPendingRequest) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  EXPECT_CALL(message_handler_, SendMediaRequest).WillOnce(Return(345));
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "v2_message",
        "clientId": "theClientId",
        "message": {
          "sessionId": "theSessionId",
          "type": "MEDIA_GET_STATUS"
        },
        "sequenceNumber": 123
      })"));
  RunUntilIdle();

  // Same as in OnMediaStatusUpdated, except there is a sequenceNumber field.
  EXPECT_CALL(*client_connection_, OnMessage(IsCastMessage(R"({
    "clientId": "theClientId",
    "message": {"foo": "bar"},
    "sequenceNumber": 123,
    "timeoutMillis": 0,
    "type": "v2_message"
  })")));
  manager_->OnMediaStatusUpdated(
      sink_, *ParseJsonDeprecated(R"({"foo": "bar"})"), 345);
}

TEST_F(CastActivityManagerTest, SendVolumeCommandToReceiver) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  // Message created by CastActivityRecord::SendVolumeCommandToReceiver().
  std::string expected_message = R"({
    "sessionId": "theSessionId",
    "type": "SET_VOLUME"
  })";
  EXPECT_CALL(message_handler_,
              SendSetVolumeRequest(kChannelId, IsJson(expected_message),
                                   "theClientId", _))
      .WillOnce(WithArg<3>([&](auto callback) {
        // Check message created by CastSessionClient::SendResultResponse().
        EXPECT_CALL(*client_connection_, OnMessage(IsCastMessage(R"({
                    "clientId": "theClientId",
                    "message": null,
                    "sequenceNumber": 123,
                    "timeoutMillis": 0,
                    "type": "v2_message"
                  })")));
        std::move(callback).Run(cast_channel::Result::kOk);
        return cast_channel::Result::kOk;
      }));
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "v2_message",
        "clientId": "theClientId",
        "sequenceNumber": 123,
        "message": {
          "sessionId": "theSessionId",
          "type": "SET_VOLUME"
        }
      })"));
}

TEST_F(CastActivityManagerTest, SendMediaRequestToReceiver) {
  LaunchSession();
  LaunchSessionResponseSuccess();

  std::string expected_message = R"({
    "sessionId": "theSessionId",
    "type": "MEDIA_GET_STATUS"
  })";
  EXPECT_CALL(message_handler_,
              SendMediaRequest(kChannelId, IsJson(expected_message),
                               "theClientId", "theTransportId"));
  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "v2_message",
        "clientId": "theClientId",
        "message": {
          "sessionId": "theSessionId",
          "type": "MEDIA_GET_STATUS"
        },
        "sequenceNumber": 123
      })"));
}

TEST_P(CastActivityManagerTest, HandleLeaveSession) {
  bool should_close;
  const char* url;
  std::tie(should_close, url) = GetParam();

  LaunchSession(url);
  LaunchSessionResponseSuccess();

  // Called via CastSessionClient::SendMessageToClient.
  EXPECT_CALL(*client_connection_, OnMessage).WillOnce([](auto msg) {
    // Verify that an acknowlegement message was sent.
    ASSERT_TRUE(msg->is_message());
    EXPECT_THAT(msg->get_message(), IsJson(R"({
          "clientId": "theClientId",
          "message": null,
          "timeoutMillis": 0,
          "type": "leave_session",
        })"));
  });

  if (should_close) {
    // Called via CastActivityRecord::HandleLeaveSession.
    EXPECT_CALL(
        *client_connection_,
        DidChangeState(blink::mojom::PresentationConnectionState::CLOSED));
  }

  client_connection_->SendMessageToMediaRouter(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "leave_session",
        "clientId": "theClientId",
        "message": {}
      })"));
}

INSTANTIATE_TEST_SUITE_P(
    Urls,
    CastActivityManagerTest,
    testing::Values(
        std::make_pair(true,
                       "cast:ABCDEFGH?clientId=theClientId&autoJoinPolicy=tab_"
                       "and_origin_scoped"),
        std::make_pair(
            true,
            "cast:ABCDEFGH?clientId=theClientId&autoJoinPolicy=origin_scoped"),
        std::make_pair(
            false,
            "cast:ABCDEFGH?clientId=theClientId&autoJoinPolicy=page_scoped"),
        std::make_pair(false, "cast:ABCDEFGH?clientId=theClientId")));

}  // namespace media_router
