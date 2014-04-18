// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

#include <deque>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace gcm {

const uint32 MAX_LOGGED_ACTIVITY_COUNT = 100;

namespace {

// Insert an itme to the front of deque while maintaining the size of the deque.
// Overflow item is discarded.
template <typename T>
T* InsertCircularBuffer(std::deque<T>* q, const T& item) {
  DCHECK(q);
  q->push_front(item);
  if (q->size() > MAX_LOGGED_ACTIVITY_COUNT) {
    q->pop_back();
  }
  return &q->front();
}

// Helper for getting string representation of the MessageSendStatus enum.
std::string GetMessageSendStatusString(
    gcm::MCSClient::MessageSendStatus status) {
  switch (status) {
    case gcm::MCSClient::QUEUED:
      return "QUEUED";
    case gcm::MCSClient::SENT:
      return "SENT";
    case gcm::MCSClient::QUEUE_SIZE_LIMIT_REACHED:
      return "QUEUE_SIZE_LIMIT_REACHED";
    case gcm::MCSClient::APP_QUEUE_SIZE_LIMIT_REACHED:
      return "APP_QUEUE_SIZE_LIMIT_REACHED";
    case gcm::MCSClient::MESSAGE_TOO_LARGE:
      return "MESSAGE_TOO_LARGE";
    case gcm::MCSClient::NO_CONNECTION_ON_ZERO_TTL:
      return "NO_CONNECTION_ON_ZERO_TTL";
    case gcm::MCSClient::TTL_EXCEEDED:
      return "TTL_EXCEEDED";
    default:
      NOTREACHED();
      return "UNKNOWN";
  }
}

}  // namespace

GCMStatsRecorder::Activity::Activity()
    : time(base::Time::Now()) {
}

GCMStatsRecorder::Activity::~Activity() {
}

GCMStatsRecorder::SendingActivity::SendingActivity() {
}

GCMStatsRecorder::SendingActivity::~SendingActivity() {
}

GCMStatsRecorder::GCMStatsRecorder() : is_recording_(false) {
}

GCMStatsRecorder::~GCMStatsRecorder() {
}

void GCMStatsRecorder::SetRecording(bool recording) {
  is_recording_ = recording;
}

void GCMStatsRecorder::Clear() {
  sending_activities_.clear();
}

void GCMStatsRecorder::CollectSendingActivities(
    std::vector<SendingActivity>* activities) const {
  activities->insert(activities->begin(),
                     sending_activities_.begin(),
                     sending_activities_.end());
}

void GCMStatsRecorder::RecordSending(const std::string& app_id,
                                     const std::string& receiver_id,
                                     const std::string& message_id,
                                     const std::string& event,
                                     const std::string& details) {
  SendingActivity data;
  SendingActivity* inserted_data = InsertCircularBuffer(
      &sending_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->receiver_id = receiver_id;
  inserted_data->message_id = message_id;
  inserted_data->event = event;
  inserted_data->details = details;
}

void GCMStatsRecorder::RecordDataSentToWire(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    int queued) {
  if (is_recording_) {
    RecordSending(app_id, receiver_id, message_id, "Data msg sent to wire",
                  base::StringPrintf("Msg queued for %d seconds", queued));
  }
}

void GCMStatsRecorder::RecordNotifySendStatus(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    gcm::MCSClient::MessageSendStatus status,
    int byte_size,
    int ttl) {
  UMA_HISTOGRAM_ENUMERATION("GCM.SendMessageStatus", status,
                            gcm::MCSClient::SEND_STATUS_COUNT);
  if (is_recording_) {
    RecordSending(
        app_id,
        receiver_id,
        message_id,
        base::StringPrintf("SEND status: %s",
                           GetMessageSendStatusString(status).c_str()),
        base::StringPrintf("Msg size: %d bytes, TTL: %d", byte_size, ttl));
  }
}

void GCMStatsRecorder::RecordIncomingSendError(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id) {
  UMA_HISTOGRAM_COUNTS("GCM.IncomingSendErrors", 1);
  if (is_recording_) {
    RecordSending(app_id, receiver_id, message_id, "Received 'send error' msg",
                  std::string());
  }
}

}  // namespace gcm
