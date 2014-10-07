// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_
#define GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_

#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

namespace gcm {

// The fake version of GCMStatsRecorder that does nothing.
class FakeGCMStatsRecorder : public GCMStatsRecorder {
 public:
  FakeGCMStatsRecorder();
  virtual ~FakeGCMStatsRecorder();

  virtual void RecordCheckinInitiated(uint64 android_id) override;
  virtual void RecordCheckinDelayedDueToBackoff(int64 delay_msec) override;
  virtual void RecordCheckinSuccess() override;
  virtual void RecordCheckinFailure(std::string status,
                                    bool will_retry) override;
  virtual void RecordConnectionInitiated(const std::string& host) override;
  virtual void RecordConnectionDelayedDueToBackoff(int64 delay_msec) override;
  virtual void RecordConnectionSuccess() override;
  virtual void RecordConnectionFailure(int network_error) override;
  virtual void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) override;
  virtual void RecordRegistrationSent(const std::string& app_id,
                                      const std::string& sender_ids) override;
  virtual void RecordRegistrationResponse(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      RegistrationRequest::Status status) override;
  virtual void RecordRegistrationRetryRequested(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids,
      int retries_left) override;
  virtual void RecordUnregistrationSent(const std::string& app_id) override;
  virtual void RecordUnregistrationResponse(
      const std::string& app_id,
      UnregistrationRequest::Status status) override;
  virtual void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                                int64 delay_msec) override;
  virtual void RecordDataMessageReceived(
      const std::string& app_id,
      const std::string& from,
      int message_byte_size,
      bool to_registered_app,
      ReceivedMessageType message_type) override;
  virtual void RecordDataSentToWire(const std::string& app_id,
                                    const std::string& receiver_id,
                                    const std::string& message_id,
                                    int queued) override;
  virtual void RecordNotifySendStatus(const std::string& app_id,
                                      const std::string& receiver_id,
                                      const std::string& message_id,
                                      MCSClient::MessageSendStatus status,
                                      int byte_size,
                                      int ttl) override;
  virtual void RecordIncomingSendError(const std::string& app_id,
                                       const std::string& receiver_id,
                                       const std::string& message_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGCMStatsRecorder);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_MONITORING_FAKE_GCM_STATS_RECODER_H_
