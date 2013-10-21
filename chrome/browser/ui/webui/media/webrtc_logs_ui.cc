// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/webrtc_logs_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_log_upload_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateWebRtcLogsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIWebRtcLogsHost);
  source->SetUseJsonJSFormatV2();

  source->AddLocalizedString("webrtcLogsTitle", IDS_WEBRTC_LOGS_TITLE);
  source->AddLocalizedString("webrtcLogCountFormat",
                             IDS_WEBRTC_LOGS_LOG_COUNT_BANNER_FORMAT);
  source->AddLocalizedString("webrtcLogHeaderFormat",
                             IDS_WEBRTC_LOGS_LOG_HEADER_FORMAT);
  source->AddLocalizedString("webrtcLogTimeFormat",
                             IDS_WEBRTC_LOGS_LOG_TIME_FORMAT);
  source->AddLocalizedString("bugLinkText", IDS_WEBRTC_LOGS_BUG_LINK_LABEL);
  source->AddLocalizedString("noLogsMessage",
                             IDS_WEBRTC_LOGS_NO_LOGS_MESSAGE);
  source->AddLocalizedString("disabledHeader", IDS_WEBRTC_LOGS_DISABLED_HEADER);
  source->AddLocalizedString("disabledMessage",
                             IDS_WEBRTC_LOGS_DISABLED_MESSAGE);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("webrtc_logs.js", IDR_WEBRTC_LOGS_JS);
  source->SetDefaultResource(IDR_WEBRTC_LOGS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// WebRtcLogsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://webrtc-logs/ page.
class WebRtcLogsDOMHandler : public WebUIMessageHandler,
                             public WebRtcLogUploadList::Delegate {
 public:
  explicit WebRtcLogsDOMHandler(Profile* profile);
  virtual ~WebRtcLogsDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // WebRtcLogUploadList::Delegate implemenation.
  virtual void OnUploadListAvailable() OVERRIDE;

 private:
  // Asynchronously fetches the list of upload WebRTC logs. Called from JS.
  void HandleRequestWebRtcLogs(const ListValue* args);

  // Sends the recently uploaded logs list JS.
  void UpdateUI();

  // Loads, parses and stores the list of uploaded WebRTC logs.
  scoped_refptr<WebRtcLogUploadList> upload_list_;

  // Set when |upload_list_| has finished populating the list of logs.
  bool list_available_;

  // Set when the webpage wants to update the list (on the webpage) but
  // |upload_list_| hasn't finished populating the list of logs yet.
  bool js_request_pending_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLogsDOMHandler);
};

WebRtcLogsDOMHandler::WebRtcLogsDOMHandler(Profile* profile)
    : list_available_(false), js_request_pending_(false) {
  upload_list_ = WebRtcLogUploadList::Create(this, profile);
}

WebRtcLogsDOMHandler::~WebRtcLogsDOMHandler() {
  upload_list_->ClearDelegate();
}

void WebRtcLogsDOMHandler::RegisterMessages() {
  upload_list_->LoadUploadListAsynchronously();

  web_ui()->RegisterMessageCallback("requestWebRtcLogsList",
      base::Bind(&WebRtcLogsDOMHandler::HandleRequestWebRtcLogs,
                 base::Unretained(this)));
}

void WebRtcLogsDOMHandler::HandleRequestWebRtcLogs(const ListValue* args) {
  if (!WebRtcLogsUI::WebRtcLogsUIEnabled() || list_available_)
    UpdateUI();
  else
    js_request_pending_ = true;
}

void WebRtcLogsDOMHandler::OnUploadListAvailable() {
  list_available_ = true;
  if (js_request_pending_)
    UpdateUI();
}

void WebRtcLogsDOMHandler::UpdateUI() {
  bool webrtc_logs_enabled = WebRtcLogsUI::WebRtcLogsUIEnabled();
  ListValue upload_list;

  if (webrtc_logs_enabled) {
    std::vector<WebRtcLogUploadList::UploadInfo> uploads;
    upload_list_->GetUploads(50, &uploads);

    for (std::vector<WebRtcLogUploadList::UploadInfo>::iterator i =
         uploads.begin(); i != uploads.end(); ++i) {
      DictionaryValue* upload = new DictionaryValue();
      upload->SetString("id", i->id);
      upload->SetString("time", base::TimeFormatFriendlyDateAndTime(i->time));
      upload_list.Append(upload);
    }
  }

  base::FundamentalValue enabled(webrtc_logs_enabled);

  const chrome::VersionInfo version_info;
  base::StringValue version(version_info.Version());

  web_ui()->CallJavascriptFunction("updateWebRtcLogsList", enabled, upload_list,
                                   version);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// WebRtcLogsUI
//
///////////////////////////////////////////////////////////////////////////////

WebRtcLogsUI::WebRtcLogsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(new WebRtcLogsDOMHandler(profile));

  // Set up the chrome://webrtc-logs/ source.
  content::WebUIDataSource::Add(profile, CreateWebRtcLogsUIHTMLSource());
}

// static
bool WebRtcLogsUI::WebRtcLogsUIEnabled() {
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  bool reporting_enabled = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &reporting_enabled);
  return reporting_enabled;
#elif defined(OS_ANDROID)
  // Android has it's own setings for metrics / crash uploading.
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(prefs::kCrashReportingEnabled);
#else
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(prefs::kMetricsReportingEnabled);
#endif
#else
  return false;
#endif
}
