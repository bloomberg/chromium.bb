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
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/engine/unregistration_request.h"

namespace gcm {

// Records GCM internal stats and activities for debugging purpose. Recording
// can be turned on/off by calling SetRecording(...) function. It is turned off
// by default.
// This class is not thread safe. It is meant to be owned by a gcm client
// instance.
class GCM_EXPORT GCMStatsRecorder {
 public:
  // Type of a received message
  enum ReceivedMessageType {
    // Data message.
    DATA_MESSAGE,
    // Message that indicates some messages have been deleted on the server.
    DELETED_MESSAGES,
  };

  // Contains data that are common to all activity kinds below.
  struct GCM_EXPORT Activity {
    Activity();
    virtual ~Activity();

    base::Time time;
    std::string event;    // A short description of the event.
    std::string details;  // Any additional detail about the event.
  };

  // Contains relevant data of a connection activity.
  struct GCM_EXPORT ConnectionActivity : Activity {
    ConnectionActivity();
    virtual ~ConnectionActivity();
  };

  // Contains relevant data of a check-in activity.
  struct GCM_EXPORT CheckinActivity : Activity {
    CheckinActivity();
    virtual ~CheckinActivity();
  };

  // Contains relevant data of a registration/unregistration step.
  struct GCM_EXPORT RegistrationActivity : Activity {
    RegistrationActivity();
    virtual ~RegistrationActivity();

    std::string app_id;
    std::string sender_ids;  // Comma separated sender ids.
  };

  // Contains relevant data of a message receiving event.
  struct GCM_EXPORT ReceivingActivity : Activity {
    ReceivingActivity();
    virtual ~ReceivingActivity();

    std::string app_id;
    std::string from;
    int message_byte_size;
  };

  // Contains relevant data of a send-message step.
  struct GCM_EXPORT SendingActivity : Activity {
    SendingActivity();
    virtual ~SendingActivity();

    std::string app_id;
    std::string receiver_id;
    std::string message_id;
  };

  struct GCM_EXPORT RecordedActivities {
    RecordedActivities();
    virtual ~RecordedActivities();

    std::vector<GCMStatsRecorder::CheckinActivity> checkin_activities;
    std::vector<GCMStatsRecorder::ConnectionActivity> connection_activities;
    std::vector<GCMStatsRecorder::RegistrationActivity> registration_activities;
    std::vector<GCMStatsRecorder::ReceivingActivity> receiving_activities;
    std::vector<GCMStatsRecorder::SendingActivity> sending_activities;
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

  // All RecordXXXX methods below will record one activity. It will be inserted
  // to the front of a queue so that entries in the queue had reverse
  // chronological order.

  // Records that a check-in has been initiated.
  void RecordCheckinInitiated(uint64 android_id);

  // Records that a check-in has been delayed due to backoff.
  void RecordCheckinDelayedDueToBackoff(int64 delay_msec);

  // Records that a check-in request has succeeded.
  void RecordCheckinSuccess();

  // Records that a check-in request has failed. If a retry will be tempted then
  // will_retry should be true.
  void RecordCheckinFailure(std::string status, bool will_retry);

  // Records that a connection to MCS has been initiated.
  void RecordConnectionInitiated(const std::string& host);

  // Records that a connection has been delayed due to backoff.
  void RecordConnectionDelayedDueToBackoff(int64 delay_msec);

  // Records that connection has been successfully established.
  void RecordConnectionSuccess();

  // Records that connection has failed with a network error code.
  void RecordConnectionFailure(int network_error);

  // Records that connection reset has been signaled.
  void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason);

  // Records that a registration request has been sent. This could be initiated
  // directly from API, or from retry logic.
  void RecordRegistrationSent(const std::string& app_id,
                              const std::string& sender_ids);

  // Records that a registration response has been received from server.
  void RecordRegistrationResponse(const std::string& app_id,
                                  const std::vector<std::string>& sender_ids,
                                  RegistrationRequest::Status status);

  // Records that a registration retry has been requested. The actual retry
  // action may not occur until some time later according to backoff logic.
  void RecordRegistrationRetryRequested(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      int retries_left);

  // Records that an unregistration request has been sent. This could be
  // initiated directly from API, or from retry logic.
  void RecordUnregistrationSent(const std::string& app_id);

  // Records that an unregistration response has been received from server.
  void RecordUnregistrationResponse(const std::string& app_id,
                                    UnregistrationRequest::Status status);

  // Records that an unregistration retry has been requested and delayed due to
  // backoff logic.
  void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                        int64 delay_msec);

  // Records that a data message has been received. If this message is not
  // sent to a registered app, to_registered_app shoudl be false. If it
  // indicates that a message has been dropped on the server, is_message_dropped
  // should be true.
  void RecordDataMessageReceived(const std::string& app_id,
                                 const std::string& from,
                                 int message_byte_size,
                                 bool to_registered_app,
                                 ReceivedMessageType message_type);

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

  // Collect all recorded activities into the struct.
  void CollectActivities(RecordedActivities* recorder_activities) const;

  const std::deque<CheckinActivity>& checkin_activities() const {
    return checkin_activities_;
  }
  const std::deque<ConnectionActivity>& connection_activities() const {
    return connection_activities_;
  }
  const std::deque<RegistrationActivity>& registration_activities() const {
    return registration_activities_;
  }
  const std::deque<ReceivingActivity>& receiving_activities() const {
    return receiving_activities_;
  }
  const std::deque<SendingActivity>& sending_activities() const {
    return sending_activities_;
  }

 protected:
  void RecordCheckin(const std::string& event,
                     const std::string& details);
  void RecordConnection(const std::string& event,
                        const std::string& details);
  void RecordRegistration(const std::string& app_id,
                          const std::string& sender_id,
                          const std::string& event,
                          const std::string& details);
  void RecordReceiving(const std::string& app_id,
                       const std::string& from,
                       int message_byte_size,
                       const std::string& event,
                       const std::string& details);
  void RecordSending(const std::string& app_id,
                     const std::string& receiver_id,
                     const std::string& message_id,
                     const std::string& event,
                     const std::string& details);

  bool is_recording_;

  std::deque<CheckinActivity> checkin_activities_;
  std::deque<ConnectionActivity> connection_activities_;
  std::deque<RegistrationActivity> registration_activities_;
  std::deque<ReceivingActivity> receiving_activities_;
  std::deque<SendingActivity> sending_activities_;

  DISALLOW_COPY_AND_ASSIGN(GCMStatsRecorder);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_GCM_STATS_RECORDER_H_
