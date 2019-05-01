// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_activity_record.h"

#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessage;
using blink::mojom::PresentationConnectionMessagePtr;
using testing::_;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::WithArg;

namespace media_router {

namespace {

constexpr int kChannelId = 42;
constexpr char kAppId[] = "theAppId";
constexpr char kRouteId[] = "theRouteId";
constexpr char kSinkId[] = "cast:<id42>";

CastMediaSource MakeSourceWithClientId(
    const std::string& client_id,
    AutoJoinPolicy auto_join_policy = AutoJoinPolicy::kPageScoped) {
  CastMediaSource source("dummySourceId", std::vector<CastAppInfo>(),
                         auto_join_policy);
  source.set_client_id(client_id);
  return source;
}

}  // namespace

class MockCastActivityManager : public CastActivityManagerBase {
 public:
  MOCK_METHOD2(MakeResultCallbackForRoute,
               cast_channel::ResultCallback(
                   const std::string& route_id,
                   mojom::MediaRouteProvider::TerminateRouteCallback callback));
};

class CastActivityRecordTest : public testing::Test {
 public:
  CastActivityRecordTest() {}

  ~CastActivityRecordTest() override = default;

  void SetUp() override {
    media_sink_service_.AddOrUpdateSink(sink_);
    ASSERT_EQ(kSinkId, sink_.id());

    session_tracker_.reset(
        new CastSessionTracker(&media_sink_service_, &message_handler_,
                               socket_service_.task_runner()));

    MediaRoute route;
    route.set_media_route_id(kRouteId);
    route.set_media_sink_id(kSinkId);
    record_.reset(new CastActivityRecord(
        route, kAppId, &media_sink_service_, &message_handler_,
        session_tracker_.get(), data_decoder_.get(), &manager_));

    std::unique_ptr<CastSession> session =
        CastSession::From(sink_, ParseJson(R"({
        "applications": [{
          "appId": "theAppId",
          "displayName": "App display name",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "theSessionId",
          "statusText": "theStatusText",
          "transportId": "theTransportId"
        }]
      })"));
    ASSERT_EQ("theSessionId", session->session_id());
    session_ = session.get();
    session_tracker_->SetSessionForTest(kSinkId, std::move(session));
  }

  void TearDown() override { RunUntilIdle(); }

 protected:
  void SetUpSession() { record_->SetOrUpdateSession(*session_, sink_, ""); }

  // Run any pending events and verify expectations associated with them.
  void RunUntilIdle() {
    thread_bundle_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&socket_service_);
    testing::Mock::VerifyAndClearExpectations(&message_handler_);
    testing::Mock::VerifyAndClearExpectations(&manager_);
  }

  MediaRoute& route() { return record_->route_; }

  // Gets an arbitrary tab ID.
  int NewTabId() { return tab_id_counter++; }

  int tab_id_counter = 239;  // Arbitrary number.

  // TODO(crbug.com/954797): Factor out members also present in
  // CastActivityManagerTest.
  content::TestBrowserThreadBundle thread_bundle_;
  MediaSinkInternal sink_ = CreateCastSink(kChannelId);
  service_manager::TestConnectorFactory connector_factory_;
  cast_channel::MockCastSocketService socket_service_{
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI})};
  cast_channel::MockCastMessageHandler message_handler_{&socket_service_};
  std::unique_ptr<DataDecoder> data_decoder_ =
      std::make_unique<DataDecoder>(connector_factory_.GetDefaultConnector());
  TestMediaSinkService media_sink_service_;
  std::unique_ptr<CastSessionTracker> session_tracker_;
  MockCastActivityManager manager_;
  CastSession* session_ = nullptr;
  std::unique_ptr<CastActivityRecord> record_;
};

TEST_F(CastActivityRecordTest, SendAppMessageToReceiver) {
  // TODO(crbug.com/954797): Test case where there is no session.
  // TODO(crbug.com/954797): Test case where message has invalid namespace.

  EXPECT_CALL(message_handler_, SendAppMessage(kChannelId, _))
      .WillOnce(Return(cast_channel::Result::kFailed))
      .WillOnce(WithArg<1>([](const cast_channel::CastMessage& cast_message) {
        EXPECT_EQ("theClientId", cast_message.source_id());
        EXPECT_EQ("theTransportId", cast_message.destination_id());
        EXPECT_EQ("urn:x-cast:com.google.foo", cast_message.namespace_());
        EXPECT_TRUE(cast_message.has_payload_utf8());
        EXPECT_THAT(cast_message.payload_utf8(), IsJson(R"({"foo": "bar"})"));
        EXPECT_FALSE(cast_message.has_payload_binary());
        return cast_channel::Result::kOk;
      }));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJson(R"({
    "type": "app_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "namespaceName": "urn:x-cast:com.google.foo",
      "sessionId": "theSessionId",
      "message": { "foo": "bar" },
    },
  })"));

  SetUpSession();
  EXPECT_EQ(cast_channel::Result::kFailed,
            record_->SendAppMessageToReceiver(*message));
  EXPECT_EQ(cast_channel::Result::kOk,
            record_->SendAppMessageToReceiver(*message));
}

TEST_F(CastActivityRecordTest, SendMediaRequestToReceiver) {
  // TODO(crbug.com/954797): Test case where there is no session.

  const base::Optional<int> request_id = 1234;

  EXPECT_CALL(
      message_handler_,
      SendMediaRequest(
          kChannelId,
          IsJson(
              R"({"sessionId": "theSessionId", "type": "theV2MessageType"})"),
          "theClientId", "theTransportId"))
      .WillOnce(Return(base::nullopt))
      .WillOnce(Return(request_id));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJson(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "type": "theV2MessageType",
      "sessionId": "theSessionId",
    },
  })"));

  SetUpSession();
  EXPECT_FALSE(record_->SendMediaRequestToReceiver(*message));
  EXPECT_EQ(request_id, record_->SendMediaRequestToReceiver(*message));
}

TEST_F(CastActivityRecordTest, SendSetVolumeRequestToReceiver) {
  // TODO(crbug.com/954797): Test case where no socket is found kChannelId.

  EXPECT_CALL(
      message_handler_,
      SendSetVolumeRequest(
          kChannelId,
          IsJson(
              R"({"sessionId": "theSessionId", "type": "theV2MessageType"})"),
          "theClientId", _))
      .WillOnce(WithArg<3>([](cast_channel::ResultCallback callback) {
        std::move(callback).Run(cast_channel::Result::kOk);
        return cast_channel::Result::kOk;
      }));

  base::MockCallback<cast_channel::ResultCallback> callback;
  EXPECT_CALL(callback, Run(cast_channel::Result::kOk));

  std::unique_ptr<CastInternalMessage> message =
      CastInternalMessage::From(ParseJson(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 999,
    "message": {
      "type": "theV2MessageType",
      "sessionId": "theSessionId",
    },
  })"));
  record_->SendSetVolumeRequestToReceiver(*message, callback.Get());
}

TEST_F(CastActivityRecordTest, SendStopSessionMessageToReceiver) {
  const base::Optional<std::string> client_id("theClientId");

  EXPECT_CALL(message_handler_,
              StopSession(kChannelId, "theSessionId", client_id, _))
      .WillOnce(WithArg<3>([](cast_channel::ResultCallback callback) {
        std::move(callback).Run(cast_channel::Result::kFailed);
      }));

  EXPECT_CALL(manager_, MakeResultCallbackForRoute(kRouteId, _))
      .WillOnce(WithArg<1>(
          [](mojom::MediaRouteProvider::TerminateRouteCallback callback) {
            return base::BindOnce(
                [](mojom::MediaRouteProvider::TerminateRouteCallback callback,
                   cast_channel::Result result) {
                  EXPECT_EQ(cast_channel::Result::kFailed, result);
                  std::move(callback).Run(
                      base::Optional<std::string>("theErrorText"),
                      RouteRequestResult::INCOGNITO_MISMATCH);
                },
                std::move(callback));
          }));

  base::MockCallback<mojom::MediaRouteProvider::TerminateRouteCallback>
      callback;
  EXPECT_CALL(callback, Run(base::Optional<std::string>("theErrorText"),
                            RouteRequestResult::INCOGNITO_MISMATCH));

  SetUpSession();
  record_->SendStopSessionMessageToReceiver(client_id, callback.Get());
}

TEST_F(CastActivityRecordTest, HandleLeaveSession) {
  const url::Origin origin;
  const int tab_id = NewTabId();

  SetUpSession();
  record_->AddClient(
      MakeSourceWithClientId("theClientId", AutoJoinPolicy::kPageScoped),
      origin, tab_id);
  record_->AddClient(
      MakeSourceWithClientId("leaving1", AutoJoinPolicy::kOriginScoped), origin,
      NewTabId());
  record_->AddClient(
      MakeSourceWithClientId("leaving2", AutoJoinPolicy::kTabAndOriginScoped),
      origin, tab_id);
  record_->AddClient(
      MakeSourceWithClientId("not_leaving1", AutoJoinPolicy::kPageScoped),
      origin, tab_id);
  record_->AddClient(
      MakeSourceWithClientId("not_leaving2", AutoJoinPolicy::kOriginScoped),
      url::Origin(), tab_id);
  record_->AddClient(MakeSourceWithClientId(
                         "not_leaving3", AutoJoinPolicy::kTabAndOriginScoped),
                     origin, NewTabId());
  record_->AddClient(MakeSourceWithClientId(
                         "not_leaving4", AutoJoinPolicy::kTabAndOriginScoped),
                     url::Origin(), tab_id);
  record_->HandleLeaveSession("theClientId");
  // TODO(crbug.com/954797): Test that CloseConnection() is called on each
  // client that is leaving.
  EXPECT_THAT(
      record_->connected_clients(),
      UnorderedElementsAre(Pair("theClientId", _), Pair("not_leaving1", _),
                           Pair("not_leaving2", _), Pair("not_leaving3", _),
                           Pair("not_leaving4", _)));
}

TEST_F(CastActivityRecordTest, SendMessageToClient) {
  SetUpSession();
  record_->AddClient(MakeSourceWithClientId("theClientId"), url::Origin(),
                     NewTabId());
  PresentationConnectionMessagePtr message =
      PresentationConnectionMessage::NewMessage("\"theMessage\"");
  record_->SendMessageToClient("theClientId", std::move(message));
  // TODO(crbug.com/954797): Test that the message is sent to the client.
}

TEST_F(CastActivityRecordTest, AddRemoveClient) {
  // TODO(crbug.com/954797): Check value returned by AddClient().

  // Adding clients works as expected.
  ASSERT_TRUE(record_->connected_clients().empty());
  ASSERT_FALSE(route().is_local());
  record_->AddClient(MakeSourceWithClientId("theClientId1"), url::Origin(),
                     NewTabId());
  // Check that adding a client causes the route to become local.
  EXPECT_TRUE(route().is_local());
  EXPECT_THAT(record_->connected_clients(),
              UnorderedElementsAre(Pair("theClientId1", _)));
  record_->AddClient(MakeSourceWithClientId("theClientId2"), url::Origin(),
                     NewTabId());
  EXPECT_TRUE(route().is_local());
  EXPECT_THAT(
      record_->connected_clients(),
      UnorderedElementsAre(Pair("theClientId1", _), Pair("theClientId2", _)));

  // Removing a non-existant client is a no-op.
  record_->RemoveClient("noSuchClient");
  EXPECT_THAT(
      record_->connected_clients(),
      UnorderedElementsAre(Pair("theClientId1", _), Pair("theClientId2", _)));

  // Removing clients works as expected.
  record_->RemoveClient("theClientId1");
  EXPECT_THAT(record_->connected_clients(),
              UnorderedElementsAre(Pair("theClientId2", _)));
  record_->RemoveClient("theClientId2");
  EXPECT_TRUE(record_->connected_clients().empty());
}

TEST_F(CastActivityRecordTest, SetOrUpdateSession) {
  record_->AddClient(MakeSourceWithClientId("theClientId1"), url::Origin(),
                     NewTabId());
  record_->AddClient(MakeSourceWithClientId("theClientId2"), url::Origin(),
                     NewTabId());

  EXPECT_EQ(base::nullopt, record_->session_id());
  route().set_description("");
  record_->SetOrUpdateSession(*session_, sink_, "");
  EXPECT_EQ("theStatusText", route().description());
  EXPECT_EQ("theSessionId", record_->session_id());
  // TODO(crbug.com/954797): Check that SendMessageToClient() is not called on
  // any client.

  route().set_description("");
  record_->SetOrUpdateSession(*session_, sink_, "theHashToken");
  EXPECT_EQ("theStatusText", route().description());
  EXPECT_EQ("theSessionId", record_->session_id());
  // TODO(crbug.com/954797): Check that SendMessageToClient() is called on every
  // client.
}

TEST_F(CastActivityRecordTest, ClosePresentationConnections) {
  record_->AddClient(MakeSourceWithClientId("theClientId1"), url::Origin(),
                     NewTabId());
  record_->AddClient(MakeSourceWithClientId("theClientId2"), url::Origin(),
                     NewTabId());
  record_->ClosePresentationConnections(
      PresentationConnectionCloseReason::CONNECTION_ERROR);
  // TODO(crbug.com/954797): Test that CloseConnection() is called on every
  // client.
}

TEST_F(CastActivityRecordTest, TerminatePresentationConnections) {
  record_->AddClient(MakeSourceWithClientId("theClientId1"), url::Origin(),
                     NewTabId());
  record_->AddClient(MakeSourceWithClientId("theClientId2"), url::Origin(),
                     NewTabId());
  record_->TerminatePresentationConnections();
  // TODO(crbug.com/954797): Test that TerminateConnection() is called on every
  // client.
}

}  // namespace media_router
