// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_client.h"

// #include <iostream>
#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/mock_log.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "chrome/browser/media/router/providers/cast/cast_activity_manager.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "chrome/browser/media/router/providers/cast/mock_cast_activity_record.h"
#include "chrome/browser/media/router/providers/cast/test_util.h"
#include "chrome/browser/media/router/providers/common/buffered_message_sender.h"
#include "chrome/browser/media/router/test/mock_mojo_media_router.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
// #include "services/data_decoder/data_decoder_service.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
// #include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
// #include "testing/gmock/include/gmock/gmock.h"
// #include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;
using testing::Return;
using testing::WithArg;

namespace media_router {

namespace {
constexpr int kTabId = 213;

class MockPresentationConnection : public blink::mojom::PresentationConnection {
 public:
  explicit MockPresentationConnection(
      mojom::RoutePresentationConnectionPtr connections)
      : binding_(this, std::move(connections->connection_request)) {}

  ~MockPresentationConnection() override = default;

  MOCK_METHOD1(OnMessage, void(blink::mojom::PresentationConnectionMessagePtr));
  MOCK_METHOD1(DidChangeState,
               void(blink::mojom::PresentationConnectionState state));
  MOCK_METHOD1(DidClose, void(blink::mojom::PresentationConnectionCloseReason));

  // NOTE: This member doesn't look like it's used for anything, but it needs to
  // exist in order for Mojo magic to work correctly.
  mojo::Binding<blink::mojom::PresentationConnection> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockPresentationConnection);
};

}  // namespace

class CastSessionClientTest : public testing::Test {
 public:
  CastSessionClientTest() { activity_.set_session_id("theSessionId"); }

  ~CastSessionClientTest() override { RunUntilIdle(); }

 protected:
  void RunUntilIdle() { thread_bundle_.RunUntilIdle(); }

  content::TestBrowserThreadBundle thread_bundle_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride parser_override_;
  service_manager::TestConnectorFactory connector_factory_;
  cast_channel::MockCastSocketService socket_service_{
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI})};
  cast_channel::MockCastMessageHandler message_handler_{&socket_service_};
  DataDecoder decoder_{connector_factory_.GetDefaultConnector()};
  url::Origin origin_;
  MediaRoute route_;
  MockCastActivityRecord activity_{route_, "theAppId"};
  std::unique_ptr<CastSessionClient> client_ =
      std::make_unique<CastSessionClient>("theClientId",
                                          origin_,
                                          kTabId,
                                          AutoJoinPolicy::kPageScoped,
                                          &decoder_,
                                          &activity_);
  std::unique_ptr<MockPresentationConnection> mock_connection_ =
      std::make_unique<MockPresentationConnection>(client_->Init());
  base::test::MockLog log_;
};

TEST_F(CastSessionClientTest, OnInvalidJson) {
  // TODO(crbug.com/905002): Check UMA calls instead of logging (here and
  // below).
  EXPECT_CALL(log_, Log(logging::LOG_ERROR, _, _, _,
                        HasSubstr("Failed to parse Cast client message")))
      .WillOnce(Return(true));  // suppress logging

  log_.StartCapturingLogs();
  client_->OnMessage(
      blink::mojom::PresentationConnectionMessage::NewMessage("invalid js"));
}

TEST_F(CastSessionClientTest, OnInvalidMessage) {
  EXPECT_CALL(log_, Log(logging::LOG_ERROR, _, _, _,
                        AllOf(HasSubstr("Failed to parse Cast client message"),
                              HasSubstr("Not a Cast message"))))
      .WillOnce(Return(true));  // suppress logging

  log_.StartCapturingLogs();
  client_->OnMessage(
      blink::mojom::PresentationConnectionMessage::NewMessage("{}"));
}

TEST_F(CastSessionClientTest, OnMessageWrongClientId) {
  EXPECT_CALL(
      log_, Log(logging::LOG_ERROR, _, _, _,
                AllOf(HasSubstr("Client ID mismatch"), HasSubstr("theClientId"),
                      HasSubstr("theWrongClientId"))))
      .WillOnce(Return(true));  // suppress logging

  log_.StartCapturingLogs();
  client_->OnMessage(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "v2_message",
        "clientId": "theWrongClientId",
        "message": {
          "sessionId": "theSessionId",
          "type": "MEDIA_GET_STATUS"
        }
      })"));
}

TEST_F(CastSessionClientTest, OnMessageWrongSessionId) {
  EXPECT_CALL(log_, Log(logging::LOG_ERROR, _, _, _,
                        AllOf(HasSubstr("Session ID mismatch"),
                              HasSubstr("theSessionId"),
                              HasSubstr("theWrongSessionId"))))
      .WillOnce(Return(true));  // suppress logging

  log_.StartCapturingLogs();
  client_->OnMessage(
      blink::mojom::PresentationConnectionMessage::NewMessage(R"({
        "type": "v2_message",
        "clientId": "theClientId",
        "message": {
          "sessionId": "theWrongSessionId",
          "type": "MEDIA_GET_STATUS"
        }
      })"));
}

TEST_F(CastSessionClientTest, AppMessageFromClient) {
  EXPECT_CALL(activity_, SendAppMessageToReceiver)
      .WillOnce(Return(cast_channel::Result::kOk));

  client_->OnMessage(
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
}

TEST_F(CastSessionClientTest, OnMediaStatusUpdatedWithPendingRequest) {
  EXPECT_CALL(activity_, SendMediaRequestToReceiver(IsCastInternalMessage(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 123,
    "message": {
       "sessionId": "theSessionId",
       "type": "MEDIA_GET_STATUS"
    }
  })")))
      .WillOnce(Return(345));
  client_->OnMessage(
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

  EXPECT_CALL(*mock_connection_, OnMessage(IsCastMessage(R"({
    "clientId": "theClientId",
    "message": {"foo": "bar"},
    "timeoutMillis": 0,
    "type": "v2_message"
  })")));
  client_->SendMediaStatusToClient(ParseJson(R"({"foo": "bar"})"), 123);
}

TEST_F(CastSessionClientTest, SendSetVolumeCommandToReceiver) {
  EXPECT_CALL(activity_,
              SendSetVolumeRequestToReceiver(IsCastInternalMessage(R"({
    "type": "v2_message",
    "clientId": "theClientId",
    "sequenceNumber": 123,
    "message": {
       "sessionId": "theSessionId",
       "type": "SET_VOLUME"
    }
  })"),
                                             _))
      .WillOnce(WithArg<1>([](auto callback) {
        std::move(callback).Run(cast_channel::Result::kOk);
      }));
  EXPECT_CALL(*mock_connection_, OnMessage(IsCastMessage(R"({
    "clientId": "theClientId",
    "message": null,
    "sequenceNumber": 123,
    "timeoutMillis": 0,
    "type": "v2_message"
  })")));

  client_->OnMessage(
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

}  // namespace media_router
