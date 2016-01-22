// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_internals_helper.h"

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/gcm_driver/gcm_activity.h"
#include "components/gcm_driver/gcm_internals_constants.h"
#include "components/gcm_driver/gcm_profile_service.h"

namespace gcm_driver {

namespace {

void SetCheckinInfo(const std::vector<gcm::CheckinActivity>& checkins,
                    base::ListValue* checkin_info) {
  for (const gcm::CheckinActivity& checkin : checkins) {
        base::ListValue* row = new base::ListValue();
    checkin_info->Append(row);

    row->AppendDouble(checkin.time.ToJsTime());
    row->AppendString(checkin.event);
    row->AppendString(checkin.details);
  }
}

void SetConnectionInfo(const std::vector<gcm::ConnectionActivity>& connections,
                       base::ListValue* connection_info) {
  for (const gcm::ConnectionActivity& connection : connections) {
    base::ListValue* row = new base::ListValue();
    connection_info->Append(row);

    row->AppendDouble(connection.time.ToJsTime());
    row->AppendString(connection.event);
    row->AppendString(connection.details);
  }
}

void SetRegistrationInfo(
    const std::vector<gcm::RegistrationActivity>& registrations,
    base::ListValue* registration_info) {
  for (const gcm::RegistrationActivity& registration : registrations) {
    base::ListValue* row = new base::ListValue();
    registration_info->Append(row);

    row->AppendDouble(registration.time.ToJsTime());
    row->AppendString(registration.app_id);
    row->AppendString(registration.source);
    row->AppendString(registration.event);
    row->AppendString(registration.details);
  }
}

void SetReceivingInfo(const std::vector<gcm::ReceivingActivity>& receives,
                      base::ListValue* receive_info) {
  for (const gcm::ReceivingActivity& receive : receives) {
    base::ListValue* row = new base::ListValue();
    receive_info->Append(row);

    row->AppendDouble(receive.time.ToJsTime());
    row->AppendString(receive.app_id);
    row->AppendString(receive.from);
    row->AppendString(base::IntToString(receive.message_byte_size));
    row->AppendString(receive.event);
    row->AppendString(receive.details);
  }
}

void SetSendingInfo(const std::vector<gcm::SendingActivity>& sends,
                    base::ListValue* send_info) {
  for (const gcm::SendingActivity& send : sends) {
    base::ListValue* row = new base::ListValue();
    send_info->Append(row);

    row->AppendDouble(send.time.ToJsTime());
    row->AppendString(send.app_id);
    row->AppendString(send.receiver_id);
    row->AppendString(send.message_id);
    row->AppendString(send.event);
    row->AppendString(send.details);
  }
}

void SetDecryptionFailureInfo(
    const std::vector<gcm::DecryptionFailureActivity>& failures,
    base::ListValue* failure_info) {
  for (const gcm::DecryptionFailureActivity& failure : failures) {
    base::ListValue* row = new base::ListValue();
    failure_info->Append(row);

    row->AppendDouble(failure.time.ToJsTime());
    row->AppendString(failure.app_id);
    row->AppendString(failure.details);
  }
}

}  // namespace

void SetGCMInternalsInfo(const gcm::GCMClient::GCMStatistics* stats,
                         gcm::GCMProfileService* profile_service,
                         PrefService* prefs,
                         base::DictionaryValue* results) {
  base::DictionaryValue* device_info = new base::DictionaryValue();
  results->Set(kDeviceInfo, device_info);

  device_info->SetBoolean(kProfileServiceCreated, profile_service != NULL);
  device_info->SetBoolean(kGcmEnabled,
                          gcm::GCMProfileService::IsGCMEnabled(prefs));
  if (stats) {
    results->SetBoolean(kIsRecording, stats->is_recording);
    device_info->SetBoolean(kGcmClientCreated, stats->gcm_client_created);
    device_info->SetString(kGcmClientState, stats->gcm_client_state);
    device_info->SetBoolean(kConnectionClientCreated,
                            stats->connection_client_created);
    device_info->SetString(kRegisteredAppIds,
                           base::JoinString(stats->registered_app_ids, ","));
    if (stats->connection_client_created)
      device_info->SetString(kConnectionState, stats->connection_state);
    if (stats->android_id > 0) {
      device_info->SetString(
          kAndroidId, base::StringPrintf("0x%" PRIx64, stats->android_id));
    }
    device_info->SetInteger(kSendQueueSize, stats->send_queue_size);
    device_info->SetInteger(kResendQueueSize, stats->resend_queue_size);

    if (stats->recorded_activities.checkin_activities.size() > 0) {
      base::ListValue* checkin_info = new base::ListValue();
      results->Set(kCheckinInfo, checkin_info);
      SetCheckinInfo(stats->recorded_activities.checkin_activities,
                     checkin_info);
    }
    if (stats->recorded_activities.connection_activities.size() > 0) {
      base::ListValue* connection_info = new base::ListValue();
      results->Set(kConnectionInfo, connection_info);
      SetConnectionInfo(stats->recorded_activities.connection_activities,
                        connection_info);
    }
    if (stats->recorded_activities.registration_activities.size() > 0) {
      base::ListValue* registration_info = new base::ListValue();
      results->Set(kRegistrationInfo, registration_info);
      SetRegistrationInfo(stats->recorded_activities.registration_activities,
                          registration_info);
    }
    if (stats->recorded_activities.receiving_activities.size() > 0) {
      base::ListValue* receive_info = new base::ListValue();
      results->Set(kReceiveInfo, receive_info);
      SetReceivingInfo(stats->recorded_activities.receiving_activities,
                       receive_info);
    }
    if (stats->recorded_activities.sending_activities.size() > 0) {
      base::ListValue* send_info = new base::ListValue();
      results->Set(kSendInfo, send_info);
      SetSendingInfo(stats->recorded_activities.sending_activities, send_info);
    }

    if (stats->recorded_activities.decryption_failure_activities.size() > 0) {
      base::ListValue* failure_info = new base::ListValue();
      results->Set(kDecryptionFailureInfo, failure_info);
      SetDecryptionFailureInfo(
          stats->recorded_activities.decryption_failure_activities,
          failure_info);
    }
  }
}

}  // namespace gcm_driver
