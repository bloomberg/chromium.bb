// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gcm_internals_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_driver.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace {

void SetCheckinInfo(
    const std::vector<gcm::CheckinActivity>& checkins,
    base::ListValue* checkin_info) {
  std::vector<gcm::CheckinActivity>::const_iterator it = checkins.begin();
  for (; it < checkins.end(); ++it) {
    base::ListValue* row = new base::ListValue();
    checkin_info->Append(row);

    row->AppendDouble(it->time.ToJsTime());
    row->AppendString(it->event);
    row->AppendString(it->details);
  }
}

void SetConnectionInfo(
    const std::vector<gcm::ConnectionActivity>& connections,
    base::ListValue* connection_info) {
  std::vector<gcm::ConnectionActivity>::const_iterator it = connections.begin();
  for (; it < connections.end(); ++it) {
    base::ListValue* row = new base::ListValue();
    connection_info->Append(row);

    row->AppendDouble(it->time.ToJsTime());
    row->AppendString(it->event);
    row->AppendString(it->details);
  }
}

void SetRegistrationInfo(
    const std::vector<gcm::RegistrationActivity>& registrations,
    base::ListValue* registration_info) {
  std::vector<gcm::RegistrationActivity>::const_iterator it =
      registrations.begin();
  for (; it < registrations.end(); ++it) {
    base::ListValue* row = new base::ListValue();
    registration_info->Append(row);

    row->AppendDouble(it->time.ToJsTime());
    row->AppendString(it->app_id);
    row->AppendString(it->sender_ids);
    row->AppendString(it->event);
    row->AppendString(it->details);
  }
}

void SetReceivingInfo(
    const std::vector<gcm::ReceivingActivity>& receives,
    base::ListValue* receive_info) {
  std::vector<gcm::ReceivingActivity>::const_iterator it = receives.begin();
  for (; it < receives.end(); ++it) {
    base::ListValue* row = new base::ListValue();
    receive_info->Append(row);

    row->AppendDouble(it->time.ToJsTime());
    row->AppendString(it->app_id);
    row->AppendString(it->from);
    row->AppendString(base::StringPrintf("%d", it->message_byte_size));
    row->AppendString(it->event);
    row->AppendString(it->details);
  }
}

void SetSendingInfo(
    const std::vector<gcm::SendingActivity>& sends,
    base::ListValue* send_info) {
  std::vector<gcm::SendingActivity>::const_iterator it = sends.begin();
  for (; it < sends.end(); ++it) {
    base::ListValue* row = new base::ListValue();
    send_info->Append(row);

    row->AppendDouble(it->time.ToJsTime());
    row->AppendString(it->app_id);
    row->AppendString(it->receiver_id);
    row->AppendString(it->message_id);
    row->AppendString(it->event);
    row->AppendString(it->details);
  }
}

// Class acting as a controller of the chrome://gcm-internals WebUI.
class GcmInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  GcmInternalsUIMessageHandler();
  virtual ~GcmInternalsUIMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Return all of the GCM related infos to the gcm-internals page by calling
  // Javascript callback function
  // |gcm-internals.returnInfo()|.
  void ReturnResults(Profile* profile, gcm::GCMProfileService* profile_service,
                     const gcm::GCMClient::GCMStatistics* stats) const;

  // Request all of the GCM related infos through gcm profile service.
  void RequestAllInfo(const base::ListValue* args);

  // Enables/disables GCM activity recording through gcm profile service.
  void SetRecording(const base::ListValue* args);

  // Callback function of the request for all gcm related infos.
  void RequestGCMStatisticsFinished(
      const gcm::GCMClient::GCMStatistics& args) const;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GcmInternalsUIMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GcmInternalsUIMessageHandler);
};

GcmInternalsUIMessageHandler::GcmInternalsUIMessageHandler()
    : weak_ptr_factory_(this) {}

GcmInternalsUIMessageHandler::~GcmInternalsUIMessageHandler() {}

void GcmInternalsUIMessageHandler::ReturnResults(
    Profile* profile,
    gcm::GCMProfileService* profile_service,
    const gcm::GCMClient::GCMStatistics* stats) const {
  base::DictionaryValue results;
  base::DictionaryValue* device_info = new base::DictionaryValue();
  results.Set("deviceInfo", device_info);

  device_info->SetBoolean("profileServiceCreated", profile_service != NULL);
  device_info->SetBoolean("gcmEnabled",
                          gcm::GCMProfileService::IsGCMEnabled(profile));
  if (profile_service) {
    device_info->SetString("signedInUserName",
                           profile_service->SignedInUserName());
  }
  if (stats) {
    results.SetBoolean("isRecording", stats->is_recording);
    device_info->SetBoolean("gcmClientCreated", stats->gcm_client_created);
    device_info->SetString("gcmClientState", stats->gcm_client_state);
    device_info->SetBoolean("connectionClientCreated",
                            stats->connection_client_created);
    device_info->SetString("registeredAppIds",
                           JoinString(stats->registered_app_ids, ","));
    if (stats->connection_client_created)
      device_info->SetString("connectionState", stats->connection_state);
    if (stats->android_id > 0) {
      device_info->SetString("androidId",
          base::StringPrintf("0x%" PRIx64, stats->android_id));
    }
    device_info->SetInteger("sendQueueSize", stats->send_queue_size);
    device_info->SetInteger("resendQueueSize", stats->resend_queue_size);

    if (stats->recorded_activities.checkin_activities.size() > 0) {
      base::ListValue* checkin_info = new base::ListValue();
      results.Set("checkinInfo", checkin_info);
      SetCheckinInfo(stats->recorded_activities.checkin_activities,
                     checkin_info);
    }
    if (stats->recorded_activities.connection_activities.size() > 0) {
      base::ListValue* connection_info = new base::ListValue();
      results.Set("connectionInfo", connection_info);
      SetConnectionInfo(stats->recorded_activities.connection_activities,
                        connection_info);
    }
    if (stats->recorded_activities.registration_activities.size() > 0) {
      base::ListValue* registration_info = new base::ListValue();
      results.Set("registrationInfo", registration_info);
      SetRegistrationInfo(stats->recorded_activities.registration_activities,
                          registration_info);
    }
    if (stats->recorded_activities.receiving_activities.size() > 0) {
      base::ListValue* receive_info = new base::ListValue();
      results.Set("receiveInfo", receive_info);
      SetReceivingInfo(stats->recorded_activities.receiving_activities,
                       receive_info);
    }
    if (stats->recorded_activities.sending_activities.size() > 0) {
      base::ListValue* send_info = new base::ListValue();
      results.Set("sendInfo", send_info);
      SetSendingInfo(stats->recorded_activities.sending_activities, send_info);
    }
  }
  web_ui()->CallJavascriptFunction("gcmInternals.setGcmInternalsInfo",
                                   results);
}

void GcmInternalsUIMessageHandler::RequestAllInfo(
    const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }
  bool clear_logs = false;
  if (!args->GetBoolean(0, &clear_logs)) {
    NOTREACHED();
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  gcm::GCMProfileService* profile_service =
    gcm::GCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service || !profile_service->driver()) {
    ReturnResults(profile, NULL, NULL);
  } else {
    profile_service->driver()->GetGCMStatistics(
        base::Bind(&GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
                   weak_ptr_factory_.GetWeakPtr()),
        clear_logs);
  }
}

void GcmInternalsUIMessageHandler::SetRecording(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }
  bool recording = false;
  if (!args->GetBoolean(0, &recording)) {
    NOTREACHED();
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  gcm::GCMProfileService* profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service) {
    ReturnResults(profile, NULL, NULL);
    return;
  }
  if (profile_service->SignedInUserName().empty()) {
    ReturnResults(profile, profile_service, NULL);
    return;
  }
  // Get fresh stats after changing recording setting.
  profile_service->driver()->SetGCMRecording(
      base::Bind(
          &GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
          weak_ptr_factory_.GetWeakPtr()),
      recording);
}

void GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished(
    const gcm::GCMClient::GCMStatistics& stats) const {
  Profile* profile = Profile::FromWebUI(web_ui());
  DCHECK(profile);
  gcm::GCMProfileService* profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  DCHECK(profile_service);
  ReturnResults(profile, profile_service, &stats);
}

void GcmInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getGcmInternalsInfo",
      base::Bind(&GcmInternalsUIMessageHandler::RequestAllInfo,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setGcmInternalsRecording",
      base::Bind(&GcmInternalsUIMessageHandler::SetRecording,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace

GCMInternalsUI::GCMInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gcm-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIGCMInternalsHost);
  html_source->SetUseJsonJSFormatV2();

  html_source->SetJsonPath("strings.js");

  // Add required resources.
  html_source->AddResourcePath("gcm_internals.css", IDR_GCM_INTERNALS_CSS);
  html_source->AddResourcePath("gcm_internals.js", IDR_GCM_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_GCM_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(new GcmInternalsUIMessageHandler());
}

GCMInternalsUI::~GCMInternalsUI() {}
