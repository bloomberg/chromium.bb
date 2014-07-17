// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_stats_recorder_impl.h"

#include <deque>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace gcm {

const uint32 MAX_LOGGED_ACTIVITY_COUNT = 100;
const int64 RECEIVED_DATA_MESSAGE_BURST_LENGTH_SECONDS = 2;

namespace {

// Insert an item to the front of deque while maintaining the size of the deque.
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

// Helper for getting string representation of the
// ConnectionFactory::ConnectionResetReason enum.
std::string GetConnectionResetReasonString(
    gcm::ConnectionFactory::ConnectionResetReason reason) {
  switch (reason) {
    case gcm::ConnectionFactory::LOGIN_FAILURE:
      return "LOGIN_FAILURE";
    case gcm::ConnectionFactory::CLOSE_COMMAND:
      return "CLOSE_COMMAND";
    case gcm::ConnectionFactory::HEARTBEAT_FAILURE:
      return "HEARTBEAT_FAILURE";
    case gcm::ConnectionFactory::SOCKET_FAILURE:
      return "SOCKET_FAILURE";
    case gcm::ConnectionFactory::NETWORK_CHANGE:
      return "NETWORK_CHANGE";
    default:
      NOTREACHED();
      return "UNKNOWN_REASON";
  }
}

// Helper for getting string representation of the RegistrationRequest::Status
// enum.
std::string GetRegistrationStatusString(
    gcm::RegistrationRequest::Status status) {
  switch (status) {
    case gcm::RegistrationRequest::SUCCESS:
      return "SUCCESS";
    case gcm::RegistrationRequest::INVALID_PARAMETERS:
      return "INVALID_PARAMETERS";
    case gcm::RegistrationRequest::INVALID_SENDER:
      return "INVALID_SENDER";
    case gcm::RegistrationRequest::AUTHENTICATION_FAILED:
      return "AUTHENTICATION_FAILED";
    case gcm::RegistrationRequest::DEVICE_REGISTRATION_ERROR:
      return "DEVICE_REGISTRATION_ERROR";
    case gcm::RegistrationRequest::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    case gcm::RegistrationRequest::URL_FETCHING_FAILED:
      return "URL_FETCHING_FAILED";
    case gcm::RegistrationRequest::HTTP_NOT_OK:
      return "HTTP_NOT_OK";
    case gcm::RegistrationRequest::RESPONSE_PARSING_FAILED:
      return "RESPONSE_PARSING_FAILED";
    case gcm::RegistrationRequest::REACHED_MAX_RETRIES:
      return "REACHED_MAX_RETRIES";
    default:
      NOTREACHED();
      return "UNKNOWN_STATUS";
  }
}

// Helper for getting string representation of the RegistrationRequest::Status
// enum.
std::string GetUnregistrationStatusString(
    gcm::UnregistrationRequest::Status status) {
  switch (status) {
    case gcm::UnregistrationRequest::SUCCESS:
      return "SUCCESS";
    case gcm::UnregistrationRequest::URL_FETCHING_FAILED:
      return "URL_FETCHING_FAILED";
    case gcm::UnregistrationRequest::NO_RESPONSE_BODY:
      return "NO_RESPONSE_BODY";
    case gcm::UnregistrationRequest::RESPONSE_PARSING_FAILED:
      return "RESPONSE_PARSING_FAILED";
    case gcm::UnregistrationRequest::INCORRECT_APP_ID:
      return "INCORRECT_APP_ID";
    case gcm::UnregistrationRequest::INVALID_PARAMETERS:
      return "INVALID_PARAMETERS";
    case gcm::UnregistrationRequest::SERVICE_UNAVAILABLE:
      return "SERVICE_UNAVAILABLE";
    case gcm::UnregistrationRequest::INTERNAL_SERVER_ERROR:
      return "INTERNAL_SERVER_ERROR";
    case gcm::UnregistrationRequest::HTTP_NOT_OK:
      return "HTTP_NOT_OK";
    case gcm::UnregistrationRequest::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    default:
      NOTREACHED();
      return "UNKNOWN_STATUS";
  }
}

}  // namespace

GCMStatsRecorderImpl::GCMStatsRecorderImpl()
    : is_recording_(false),
      delegate_(NULL) {
}

GCMStatsRecorderImpl::~GCMStatsRecorderImpl() {
}

void GCMStatsRecorderImpl::SetRecording(bool recording) {
  is_recording_ = recording;
}

void GCMStatsRecorderImpl::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void GCMStatsRecorderImpl::Clear() {
  checkin_activities_.clear();
  connection_activities_.clear();
  registration_activities_.clear();
  receiving_activities_.clear();
  sending_activities_.clear();
}

void GCMStatsRecorderImpl::NotifyActivityRecorded() {
  if (delegate_)
    delegate_->OnActivityRecorded();
}

void GCMStatsRecorderImpl::RecordCheckin(
    const std::string& event,
    const std::string& details) {
  CheckinActivity data;
  CheckinActivity* inserted_data = InsertCircularBuffer(
      &checkin_activities_, data);
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordCheckinInitiated(uint64 android_id) {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin initiated",
                base::StringPrintf("Android Id: %" PRIu64, android_id));
}

void GCMStatsRecorderImpl::RecordCheckinDelayedDueToBackoff(int64 delay_msec) {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin backoff",
                base::StringPrintf("Delayed for %" PRId64 " msec",
                                   delay_msec));
}

void GCMStatsRecorderImpl::RecordCheckinSuccess() {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin succeeded", std::string());
}

void GCMStatsRecorderImpl::RecordCheckinFailure(std::string status,
                                            bool will_retry) {
  if (!is_recording_)
    return;
  RecordCheckin("Checkin failed", base::StringPrintf(
      "%s.%s",
      status.c_str(),
      will_retry ? " Will retry." : "Will not retry."));
}

void GCMStatsRecorderImpl::RecordConnection(
    const std::string& event,
    const std::string& details) {
  ConnectionActivity data;
  ConnectionActivity* inserted_data = InsertCircularBuffer(
      &connection_activities_, data);
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordConnectionInitiated(const std::string& host) {
  if (!is_recording_)
    return;
  RecordConnection("Connection initiated", host);
}

void GCMStatsRecorderImpl::RecordConnectionDelayedDueToBackoff(
    int64 delay_msec) {
  if (!is_recording_)
    return;
  RecordConnection("Connection backoff",
                   base::StringPrintf("Delayed for %" PRId64 " msec",
                                      delay_msec));
}

void GCMStatsRecorderImpl::RecordConnectionSuccess() {
  if (!is_recording_)
    return;
  RecordConnection("Connection succeeded", std::string());
}

void GCMStatsRecorderImpl::RecordConnectionFailure(int network_error) {
  if (!is_recording_)
    return;
  RecordConnection("Connection failed",
                   base::StringPrintf("With network error %d", network_error));
}

void GCMStatsRecorderImpl::RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) {
  if (!is_recording_)
    return;
  RecordConnection("Connection reset",
                   GetConnectionResetReasonString(reason));
}

void GCMStatsRecorderImpl::RecordRegistration(
    const std::string& app_id,
    const std::string& sender_ids,
    const std::string& event,
    const std::string& details) {
  RegistrationActivity data;
  RegistrationActivity* inserted_data = InsertCircularBuffer(
      &registration_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->sender_ids = sender_ids;
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordRegistrationSent(
    const std::string& app_id,
    const std::string& sender_ids) {
  UMA_HISTOGRAM_COUNTS("GCM.RegistrationRequest", 1);
  if (!is_recording_)
    return;
  RecordRegistration(app_id, sender_ids,
                     "Registration request sent", std::string());
}

void GCMStatsRecorderImpl::RecordRegistrationResponse(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    RegistrationRequest::Status status) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id, JoinString(sender_ids, ","),
                     "Registration response received",
                     GetRegistrationStatusString(status));
}

void GCMStatsRecorderImpl::RecordRegistrationRetryRequested(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    int retries_left) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id, JoinString(sender_ids, ","),
                     "Registration retry requested",
                     base::StringPrintf("Retries left: %d", retries_left));
}

void GCMStatsRecorderImpl::RecordUnregistrationSent(
    const std::string& app_id) {
  UMA_HISTOGRAM_COUNTS("GCM.UnregistrationRequest", 1);
  if (!is_recording_)
    return;
  RecordRegistration(app_id, std::string(), "Unregistration request sent",
                     std::string());
}

void GCMStatsRecorderImpl::RecordUnregistrationResponse(
    const std::string& app_id,
    UnregistrationRequest::Status status) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id,
                     std::string(),
                     "Unregistration response received",
                     GetUnregistrationStatusString(status));
}

void GCMStatsRecorderImpl::RecordUnregistrationRetryDelayed(
    const std::string& app_id,
    int64 delay_msec) {
  if (!is_recording_)
    return;
  RecordRegistration(app_id,
                     std::string(),
                     "Unregistration retry delayed",
                     base::StringPrintf("Delayed for %" PRId64 " msec",
                                        delay_msec));
}

void GCMStatsRecorderImpl::RecordReceiving(
    const std::string& app_id,
    const std::string& from,
    int message_byte_size,
    const std::string& event,
    const std::string& details) {
  ReceivingActivity data;
  ReceivingActivity* inserted_data = InsertCircularBuffer(
      &receiving_activities_, data);
  inserted_data->app_id = app_id;
  inserted_data->from = from;
  inserted_data->message_byte_size = message_byte_size;
  inserted_data->event = event;
  inserted_data->details = details;
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordDataMessageReceived(
    const std::string& app_id,
    const std::string& from,
    int message_byte_size,
    bool to_registered_app,
    ReceivedMessageType message_type) {
  if (to_registered_app)
    UMA_HISTOGRAM_COUNTS("GCM.DataMessageReceived", 1);

  base::Time new_timestamp = base::Time::Now();
  if (last_received_data_message_burst_start_time_.is_null()) {
    last_received_data_message_burst_start_time_ = new_timestamp;
  } else if ((new_timestamp - last_received_data_message_burst_start_time_) >=
             base::TimeDelta::FromSeconds(
                 RECEIVED_DATA_MESSAGE_BURST_LENGTH_SECONDS)) {
    UMA_HISTOGRAM_COUNTS(
        "GCM.DataMessageBurstReceivedIntervalSeconds",
        (new_timestamp - last_received_data_message_burst_start_time_)
            .InSeconds());
    last_received_data_message_burst_start_time_ = new_timestamp;
  }

  if (!is_recording_)
    return;
  if (!to_registered_app) {
    RecordReceiving(app_id, from, message_byte_size, "Data msg received",
                    to_registered_app ? std::string() :
                                        "No such registered app found");
  } else {
    switch(message_type) {
      case GCMStatsRecorderImpl::DATA_MESSAGE:
        RecordReceiving(app_id, from, message_byte_size, "Data msg received",
                        std::string());
        break;
      case GCMStatsRecorderImpl::DELETED_MESSAGES:
        RecordReceiving(app_id, from, message_byte_size, "Data msg received",
                        "Message has been deleted on server");
        break;
      default:
        NOTREACHED();
    }
  }
}

void GCMStatsRecorderImpl::CollectActivities(
    RecordedActivities* recorder_activities) const {
  recorder_activities->checkin_activities.insert(
      recorder_activities->checkin_activities.begin(),
      checkin_activities_.begin(),
      checkin_activities_.end());
  recorder_activities->connection_activities.insert(
      recorder_activities->connection_activities.begin(),
      connection_activities_.begin(),
      connection_activities_.end());
  recorder_activities->registration_activities.insert(
      recorder_activities->registration_activities.begin(),
      registration_activities_.begin(),
      registration_activities_.end());
  recorder_activities->receiving_activities.insert(
      recorder_activities->receiving_activities.begin(),
      receiving_activities_.begin(),
      receiving_activities_.end());
  recorder_activities->sending_activities.insert(
      recorder_activities->sending_activities.begin(),
      sending_activities_.begin(),
      sending_activities_.end());
}

void GCMStatsRecorderImpl::RecordSending(const std::string& app_id,
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
  NotifyActivityRecorded();
}

void GCMStatsRecorderImpl::RecordDataSentToWire(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    int queued) {
  if (!is_recording_)
    return;
  RecordSending(app_id, receiver_id, message_id, "Data msg sent to wire",
                base::StringPrintf("Msg queued for %d seconds", queued));
}

void GCMStatsRecorderImpl::RecordNotifySendStatus(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id,
    gcm::MCSClient::MessageSendStatus status,
    int byte_size,
    int ttl) {
  UMA_HISTOGRAM_ENUMERATION("GCM.SendMessageStatus", status,
                            gcm::MCSClient::SEND_STATUS_COUNT);
  if (!is_recording_)
    return;
  RecordSending(
      app_id,
      receiver_id,
      message_id,
      base::StringPrintf("SEND status: %s",
                         GetMessageSendStatusString(status).c_str()),
      base::StringPrintf("Msg size: %d bytes, TTL: %d", byte_size, ttl));
}

void GCMStatsRecorderImpl::RecordIncomingSendError(
    const std::string& app_id,
    const std::string& receiver_id,
    const std::string& message_id) {
  UMA_HISTOGRAM_COUNTS("GCM.IncomingSendErrors", 1);
  if (!is_recording_)
    return;
  RecordSending(app_id, receiver_id, message_id, "Received 'send error' msg",
                std::string());
}

}  // namespace gcm
