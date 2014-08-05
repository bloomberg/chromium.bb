// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_IMPL_H_
#define COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_IMPL_H_

#include <deque>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/gcm_driver/gcm_activity.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/engine/unregistration_request.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

namespace gcm {

// Records GCM internal stats and activities for debugging purpose. Recording
// can be turned on/off by calling SetRecording(...) function. It is turned off
// by default.
// This class is not thread safe. It is meant to be owned by a gcm client
// instance.
class GCMStatsRecorderImpl : public GCMStatsRecorder {
 public:
  GCMStatsRecorderImpl();
  virtual ~GCMStatsRecorderImpl();

  // Indicates whether the recorder is currently recording activities or not.
  bool is_recording() const {
    return is_recording_;
  }

  // Turns recording on/off.
  void SetRecording(bool recording);

  // Set a delegate to receive callback from the recorder.
  void SetDelegate(Delegate* delegate);

  // Clear all recorded activities.
  void Clear();

  // GCMStatsRecorder implementation:
  virtual void RecordCheckinInitiated(uint64 android_id) OVERRIDE;
  virtual void RecordCheckinDelayedDueToBackoff(int64 delay_msec) OVERRIDE;
  virtual void RecordCheckinSuccess() OVERRIDE;
  virtual void RecordCheckinFailure(std::string status,
                                    bool will_retry) OVERRIDE;
  virtual void RecordConnectionInitiated(const std::string& host) OVERRIDE;
  virtual void RecordConnectionDelayedDueToBackoff(int64 delay_msec) OVERRIDE;
  virtual void RecordConnectionSuccess() OVERRIDE;
  virtual void RecordConnectionFailure(int network_error) OVERRIDE;
  virtual void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) OVERRIDE;
  virtual void RecordRegistrationSent(const std::string& app_id,
                                      const std::string& sender_ids) OVERRIDE;
  virtual void RecordRegistrationResponse(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      RegistrationRequest::Status status) OVERRIDE;
  virtual void RecordRegistrationRetryRequested(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      int retries_left) OVERRIDE;
  virtual void RecordUnregistrationSent(const std::string& app_id) OVERRIDE;
  virtual void RecordUnregistrationResponse(
      const std::string& app_id,
      UnregistrationRequest::Status status) OVERRIDE;
  virtual void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                                int64 delay_msec) OVERRIDE;
  virtual void RecordDataMessageReceived(
      const std::string& app_id,
      const std::string& from,
      int message_byte_size,
      bool to_registered_app,
      ReceivedMessageType message_type) OVERRIDE;
  virtual void RecordDataSentToWire(const std::string& app_id,
                                    const std::string& receiver_id,
                                    const std::string& message_id,
                                    int queued) OVERRIDE;
  virtual void RecordNotifySendStatus(const std::string& app_id,
                                      const std::string& receiver_id,
                                      const std::string& message_id,
                                      MCSClient::MessageSendStatus status,
                                      int byte_size,
                                      int ttl) OVERRIDE;
  virtual void RecordIncomingSendError(const std::string& app_id,
                                       const std::string& receiver_id,
                                       const std::string& message_id) OVERRIDE;

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
  // Notify the recorder delegate, if it exists, that an activity has been
  // recorded.
  void NotifyActivityRecorded();

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
  Delegate* delegate_;

  std::deque<CheckinActivity> checkin_activities_;
  std::deque<ConnectionActivity> connection_activities_;
  std::deque<RegistrationActivity> registration_activities_;
  std::deque<ReceivingActivity> receiving_activities_;
  std::deque<SendingActivity> sending_activities_;

  base::TimeTicks last_connection_initiation_time_;
  base::TimeTicks last_connection_success_time_;
  bool data_message_received_since_connected_;
  base::TimeTicks last_received_data_message_burst_start_time_;
  base::TimeTicks last_received_data_message_time_within_burst_;
  int64 received_data_message_burst_size_;

  DISALLOW_COPY_AND_ASSIGN(GCMStatsRecorderImpl);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_IMPL_H_
