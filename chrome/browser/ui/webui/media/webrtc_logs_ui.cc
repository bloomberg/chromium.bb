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
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_log_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upload_list.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

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
  source->AddLocalizedString("webrtcLogLocalFileLabelFormat",
                             IDS_WEBRTC_LOGS_LOG_LOCAL_FILE_LABEL_FORMAT);
  source->AddLocalizedString("webrtcLogLocalFileFormat",
                             IDS_WEBRTC_LOGS_LOG_LOCAL_FILE_FORMAT);
  source->AddLocalizedString("noLocalLogFileMessage",
                             IDS_WEBRTC_LOGS_NO_LOCAL_LOG_FILE_MESSAGE);
  source->AddLocalizedString("webrtcLogUploadTimeFormat",
                             IDS_WEBRTC_LOGS_LOG_UPLOAD_TIME_FORMAT);
  source->AddLocalizedString("webrtcLogReportIdFormat",
                             IDS_WEBRTC_LOGS_LOG_REPORT_ID_FORMAT);
  source->AddLocalizedString("bugLinkText", IDS_WEBRTC_LOGS_BUG_LINK_LABEL);
  source->AddLocalizedString("webrtcLogNotUploadedMessage",
                             IDS_WEBRTC_LOGS_LOG_NOT_UPLOADED_MESSAGE);
  source->AddLocalizedString("noLogsMessage",
                             IDS_WEBRTC_LOGS_NO_LOGS_MESSAGE);
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
                             public UploadList::Delegate {
 public:
  explicit WebRtcLogsDOMHandler(Profile* profile);
  virtual ~WebRtcLogsDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // UploadList::Delegate implemenation.
  virtual void OnUploadListAvailable() OVERRIDE;

 private:
  // Asynchronously fetches the list of upload WebRTC logs. Called from JS.
  void HandleRequestWebRtcLogs(const base::ListValue* args);

  // Sends the recently uploaded logs list JS.
  void UpdateUI();

  // Loads, parses and stores the list of uploaded WebRTC logs.
  scoped_refptr<UploadList> upload_list_;

  // The directory where the logs are stored.
  base::FilePath log_dir_;

  // Set when |upload_list_| has finished populating the list of logs.
  bool list_available_;

  // Set when the webpage wants to update the list (on the webpage) but
  // |upload_list_| hasn't finished populating the list of logs yet.
  bool js_request_pending_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLogsDOMHandler);
};

WebRtcLogsDOMHandler::WebRtcLogsDOMHandler(Profile* profile)
    : log_dir_(
          WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile->GetPath())),
      list_available_(false),
      js_request_pending_(false) {
  upload_list_ = WebRtcLogList::CreateWebRtcLogList(this, profile);
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

void WebRtcLogsDOMHandler::HandleRequestWebRtcLogs(
    const base::ListValue* args) {
  if (list_available_)
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
  std::vector<UploadList::UploadInfo> uploads;
  upload_list_->GetUploads(50, &uploads);

  base::ListValue upload_list;
  for (std::vector<UploadList::UploadInfo>::iterator i = uploads.begin();
       i != uploads.end();
       ++i) {
    base::DictionaryValue* upload = new base::DictionaryValue();
    upload->SetString("id", i->id);

    base::string16 value_w;
    if (!i->time.is_null())
      value_w = base::TimeFormatFriendlyDateAndTime(i->time);
    upload->SetString("upload_time", value_w);

    value_w.clear();
    double seconds_since_epoch;
    if (base::StringToDouble(i->local_id, &seconds_since_epoch)) {
      base::Time capture_time = base::Time::FromDoubleT(seconds_since_epoch);
      value_w = base::TimeFormatFriendlyDateAndTime(capture_time);
    }
    upload->SetString("capture_time", value_w);

    base::FilePath::StringType value;
    if (!i->local_id.empty())
      value = log_dir_.AppendASCII(i->local_id)
          .AddExtension(FILE_PATH_LITERAL(".gz")).value();
    upload->SetString("local_file", value);

    upload_list.Append(upload);
  }

  const chrome::VersionInfo version_info;
  base::StringValue version(version_info.Version());

  web_ui()->CallJavascriptFunction("updateWebRtcLogsList", upload_list,
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
