// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gcm_internals_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gcm/gcm_client.h"
#include "grit/browser_resources.h"

namespace {

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
  device_info->SetString("gcmEnabledState",
      gcm::GCMProfileService::GetGCMEnabledStateString(
          gcm::GCMProfileService::GetGCMEnabledState(profile)));
  if (profile_service) {
    device_info->SetString("signedInUserName",
                           profile_service->SignedInUserName());
    device_info->SetBoolean("gcmClientReady",
                            profile_service->IsGCMClientReady());
  }
  if (stats) {
    device_info->SetBoolean("gcmClientCreated", stats->gcm_client_created);
    device_info->SetString("gcmClientState", stats->gcm_client_state);
    device_info->SetBoolean("connectionClientCreated",
                            stats->connection_client_created);
    if (stats->connection_client_created)
      device_info->SetString("connectionState", stats->connection_state);
    if (stats->android_id > 0) {
      device_info->SetString("androidId",
          base::StringPrintf("0x%" PRIx64, stats->android_id));
    }
  }
  web_ui()->CallJavascriptFunction("gcmInternals.setGcmInternalsInfo",
                                   results);
}

void GcmInternalsUIMessageHandler::RequestAllInfo(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  gcm::GCMProfileService* profile_service =
    gcm::GCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service) {
    ReturnResults(profile, NULL, NULL);
  } else {
    profile_service->RequestGCMStatistics(base::Bind(
        &GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
        weak_ptr_factory_.GetWeakPtr()));
  }
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
                 base::Unretained(this)));
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
