// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"

#include "base/json/json_reader.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media_router {

namespace {

constexpr char kSessionId[] = "sessionId";

constexpr char kReceiverStatus[] = R"({
    "status": {
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
  }
})";

// Receiver status for the backdrop (idle) app.
constexpr char kIdleReceiverStatus[] = R"({
    "status": {
        "applications": [{
          "appId": "E8C28D3C",
          "displayName": "Backdrop",
          "namespaces": [
            {"name": "urn:x-cast:com.google.cast.media"},
            {"name": "urn:x-cast:com.google.foo"}
          ],
          "sessionId": "sessionId",
          "statusText":"App status",
          "transportId":"transportId"
        }]
  }
})";

}  // namespace

class MockCastSessionObserver : public CastSessionTracker::Observer {
 public:
  MockCastSessionObserver() = default;
  ~MockCastSessionObserver() override = default;

  MOCK_METHOD2(OnSessionAddedOrUpdated,
               void(const MediaSinkInternal& sink, const CastSession& session));
  MOCK_METHOD1(OnSessionRemoved, void(const MediaSinkInternal& sink));
};

class CastSessionTrackerTest : public testing::Test {
 public:
  CastSessionTrackerTest()
      : socket_service_(base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI})),
        message_handler_(&socket_service_),
        session_tracker_(&media_sink_service_,
                         &message_handler_,
                         socket_service_.task_runner()) {
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override { session_tracker_.AddObserver(&observer_); }

  void TearDown() override { session_tracker_.RemoveObserver(&observer_); }

  void AddSinkAndSendReceiverStatusResponse() {
    EXPECT_CALL(message_handler_,
                RequestReceiverStatus(sink_.cast_data().cast_channel_id));
    media_sink_service_.AddOrUpdateSink(sink_);

    auto receiver_status = base::JSONReader::Read(kReceiverStatus);
    ASSERT_TRUE(receiver_status);
    cast_channel::InternalMessage receiver_status_message(
        cast_channel::CastMessageType::kReceiverStatus,
        std::move(*receiver_status));

    EXPECT_CALL(observer_, OnSessionAddedOrUpdated(sink_, _));
    session_tracker_.OnInternalMessage(sink_.cast_data().cast_channel_id,
                                       receiver_status_message);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastMessageHandler message_handler_;

  TestMediaSinkService media_sink_service_;
  CastSessionTracker session_tracker_;

  MockCastSessionObserver observer_;

  MediaSinkInternal sink_ = CreateCastSink(1);
};

TEST_F(CastSessionTrackerTest, QueryReceiverOnSinkAdded) {
  AddSinkAndSendReceiverStatusResponse();

  // Receiver status is sent again when sinks is updated.
  sink_.cast_data().cast_channel_id = 2;
  EXPECT_CALL(message_handler_,
              RequestReceiverStatus(sink_.cast_data().cast_channel_id));
  media_sink_service_.AddOrUpdateSink(sink_);
}

TEST_F(CastSessionTrackerTest, RemoveSessionOnSinkRemoved) {
  AddSinkAndSendReceiverStatusResponse();

  EXPECT_CALL(observer_, OnSessionRemoved(sink_));
  media_sink_service_.RemoveSink(sink_);
}

TEST_F(CastSessionTrackerTest, RemoveSession) {
  AddSinkAndSendReceiverStatusResponse();

  auto receiver_status = base::JSONReader::Read(kIdleReceiverStatus);
  ASSERT_TRUE(receiver_status);
  cast_channel::InternalMessage receiver_status_message(
      cast_channel::CastMessageType::kReceiverStatus,
      std::move(*receiver_status));

  EXPECT_CALL(observer_, OnSessionRemoved(sink_));
  session_tracker_.OnInternalMessage(sink_.cast_data().cast_channel_id,
                                     receiver_status_message);
}

TEST_F(CastSessionTrackerTest, sessions_by_sink_id) {
  EXPECT_TRUE(session_tracker_.sessions_by_sink_id().empty());

  AddSinkAndSendReceiverStatusResponse();

  const auto& sessions = session_tracker_.sessions_by_sink_id();
  EXPECT_EQ(1u, sessions.size());
  auto it = sessions.find(sink_.sink().id());
  ASSERT_TRUE(it != sessions.end());
  EXPECT_EQ(kSessionId, it->second->session_id());

  EXPECT_TRUE(session_tracker_.GetSessionById(kSessionId));
}

}  // namespace media_router
