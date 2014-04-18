// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

#include <deque>
#include <string>

#include "google_apis/gcm/engine/mcs_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

static const char kAppId[] = "app id 1";
static const char kReceiverId[] = "receiver 1";
static const char kMessageId[] = "message id 1";
static const int kQueuedSec = 5;
static const gcm::MCSClient::MessageSendStatus kMessageSendStatus =
    gcm::MCSClient::QUEUED;
static const int kByteSize = 99;
static const int kTTL = 7;

static const char kDataSentToWireEvent[] = "Data msg sent to wire";
static const char kSentToWireDetails[] = "Msg queued for 5 seconds";
static const char kNotifySendStatusEvent[] = "SEND status: QUEUED";
static const char kNotifySendStatusDetails[] = "Msg size: 99 bytes, TTL: 7";
static const char kIncomingSendErrorEvent[] = "Received 'send error' msg";
static const char kIncomingSendErrorDetails[] = "";

}  // namespace

class GCMStatsRecorderTest : public testing::Test {
 public:
  GCMStatsRecorderTest();
  virtual ~GCMStatsRecorderTest();
  virtual void SetUp() OVERRIDE;

  void VerifyRecordedSendingCount(int expected_count) {
    EXPECT_EQ(expected_count,
              static_cast<int>(recorder_.sending_activities().size()));
  }

  void VerifyDataSentToWire(const std::string& remark){
    VerifyData(recorder_.sending_activities(),
               kDataSentToWireEvent,
               kSentToWireDetails,
               remark);
  }

  void VerifyNotifySendStatus(const std::string& remark){
    VerifyData(recorder_.sending_activities(),
               kNotifySendStatusEvent,
               kNotifySendStatusDetails,
               remark);
  }

  void VerifyIncomingSendError(const std::string& remark){
    VerifyData(recorder_.sending_activities(),
               kIncomingSendErrorEvent,
               kIncomingSendErrorDetails,
               remark);
  }

 protected:
  template <typename T>
  void VerifyData(const std::deque<T>& queue, const std::string& event,
                  const std::string& details, const std::string& remark) {
    EXPECT_EQ(kAppId, queue.front().app_id) << remark;
    EXPECT_EQ(kReceiverId, queue.front().receiver_id) << remark;
    EXPECT_EQ(kMessageId, queue.front().message_id) << remark;
    EXPECT_EQ(event, queue.front().event) << remark;
    EXPECT_EQ(details, queue.front().details) << remark;
  }

  GCMStatsRecorder recorder_;
};

GCMStatsRecorderTest::GCMStatsRecorderTest(){
}

GCMStatsRecorderTest::~GCMStatsRecorderTest() {}

void GCMStatsRecorderTest::SetUp(){
  recorder_.SetRecording(true);
}

TEST_F(GCMStatsRecorderTest, StartStopRecordingTest) {
  EXPECT_TRUE(recorder_.is_recording());
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("1st call");

  recorder_.SetRecording(false);
  EXPECT_FALSE(recorder_.is_recording());
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("2nd call");
}

TEST_F(GCMStatsRecorderTest, ClearLogTest) {
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("1st call");

  recorder_.RecordNotifySendStatus(kAppId, kReceiverId, kMessageId,
                                   kMessageSendStatus, kByteSize, kTTL);
  VerifyRecordedSendingCount(2);
  VerifyNotifySendStatus("2nd call");

  recorder_.Clear();
  VerifyRecordedSendingCount(0);
}

TEST_F(GCMStatsRecorderTest, RecordSendingTest) {
  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(1);
  VerifyDataSentToWire("1st call");

  recorder_.RecordNotifySendStatus(kAppId, kReceiverId, kMessageId,
                                   kMessageSendStatus, kByteSize, kTTL);
  VerifyRecordedSendingCount(2);
  VerifyNotifySendStatus("2nd call");

  recorder_.RecordIncomingSendError(kAppId, kReceiverId, kMessageId);
  VerifyRecordedSendingCount(3);
  VerifyIncomingSendError("3rd call");

  recorder_.RecordDataSentToWire(kAppId, kReceiverId, kMessageId, kQueuedSec);
  VerifyRecordedSendingCount(4);
  VerifyDataSentToWire("4th call");
}

}  // namespace gcm
