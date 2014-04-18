// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_GCM_STATS_RECORDER_H_
#define GOOGLE_APIS_GCM_GCM_STATS_RECORDER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/mcs_client.h"

namespace gcm {

// Records GCM internal stats and activities for debugging purpose. Recording
// can be turned on/off by calling SetRecording(...) function. It is turned off
// by default.
// This class is not thread safe. It is meant to be owned by a gcm client
// instance.
class GCM_EXPORT GCMStatsRecorder {
 public:
  // Contains data that are common to all activity kinds below.
  struct GCM_EXPORT Activity {
    Activity();
    virtual ~Activity();

    base::Time time;
    std::string event;    // A short description of the event.
    std::string details;  // Any additional detail about the event.
  };

  // Contains relevant data of a send-message step.
  struct GCM_EXPORT SendingActivity : Activity {
    SendingActivity();
    virtual ~SendingActivity();

    std::string app_id;
    std::string receiver_id;
    std::string message_id;
  };

  GCMStatsRecorder();
  virtual ~GCMStatsRecorder();

  // Indicates whether the recorder is currently recording activities or not.
  bool is_recording() const {
    return is_recording_;
  }

  // Turns recording on/off.
  void SetRecording(bool recording);

  // Clear all recorded activities.
  void Clear();

  // Records that an outgoing data message was sent over the wire.
  void RecordDataSentToWire(const std::string& app_id,
                            const std::string& receiver_id,
                            const std::string& message_id,
                            int queued);
  // Records that the MCS client sent a 'send status' notification to callback.
  void RecordNotifySendStatus(const std::string& app_id,
                              const std::string& receiver_id,
                              const std::string& message_id,
                              MCSClient::MessageSendStatus status,
                              int byte_size,
                              int ttl);
  // Records that a 'send error' message was received.
  void RecordIncomingSendError(const std::string& app_id,
                               const std::string& receiver_id,
                               const std::string& message_id);

  // Records that a sending activity has occurred. It will be inserted to the
  // front of a queue ao that entries in the queue had reverse chronological
  // order.
  void CollectSendingActivities(std::vector<SendingActivity>* activities) const;

  const std::deque<SendingActivity>& sending_activities() const {
    return sending_activities_;
  }

 protected:
  void RecordSending(const std::string& app_id,
                     const std::string& receiver_id,
                     const std::string& message_id,
                     const std::string& event,
                     const std::string& details);

  bool is_recording_;

  std::deque<SendingActivity> sending_activities_;

  DISALLOW_COPY_AND_ASSIGN(GCMStatsRecorder);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_STATS_RECORDER_H_
