// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/crashes_ui.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateCrashesUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICrashesHost);
  source->SetUseJsonJSFormatV2();

  source->AddLocalizedString("shortProductName", IDS_SHORT_PRODUCT_NAME);
  source->AddLocalizedString("crashesTitle", IDS_CRASHES_TITLE);
  source->AddLocalizedString("crashCountFormat",
                             IDS_CRASHES_CRASH_COUNT_BANNER_FORMAT);
  source->AddLocalizedString("crashHeaderFormat",
                             IDS_CRASHES_CRASH_HEADER_FORMAT);
  source->AddLocalizedString("crashTimeFormat", IDS_CRASHES_CRASH_TIME_FORMAT);
  source->AddLocalizedString("bugLinkText", IDS_CRASHES_BUG_LINK_LABEL);
  source->AddLocalizedString("noCrashesMessage",
                             IDS_CRASHES_NO_CRASHES_MESSAGE);
  source->AddLocalizedString("disabledHeader", IDS_CRASHES_DISABLED_HEADER);
  source->AddLocalizedString("disabledMessage", IDS_CRASHES_DISABLED_MESSAGE);
  source->AddLocalizedString("uploadCrashesLinkText",
                             IDS_CRASHES_UPLOAD_MESSAGE);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("crashes.js", IDR_CRASHES_JS);
  source->SetDefaultResource(IDR_CRASHES_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// CrashesDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://crashes/ page.
class CrashesDOMHandler : public WebUIMessageHandler,
                          public CrashUploadList::Delegate {
 public:
  CrashesDOMHandler();
  virtual ~CrashesDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // CrashUploadList::Delegate implemenation.
  virtual void OnUploadListAvailable() OVERRIDE;

 private:
  // Asynchronously fetches the list of crashes. Called from JS.
  void HandleRequestCrashes(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Asynchronously triggers crash uploading. Called from JS.
  void HandleRequestUploads(const base::ListValue* args);
#endif

  // Sends the recent crashes list JS.
  void UpdateUI();

  scoped_refptr<CrashUploadList> upload_list_;
  bool list_available_;
  bool first_load_;

  DISALLOW_COPY_AND_ASSIGN(CrashesDOMHandler);
};

CrashesDOMHandler::CrashesDOMHandler()
    : list_available_(false), first_load_(true) {
  upload_list_ = CrashUploadList::Create(this);
}

CrashesDOMHandler::~CrashesDOMHandler() {
  upload_list_->ClearDelegate();
}

void CrashesDOMHandler::RegisterMessages() {
  upload_list_->LoadUploadListAsynchronously();
  web_ui()->RegisterMessageCallback("requestCrashList",
      base::Bind(&CrashesDOMHandler::HandleRequestCrashes,
                 base::Unretained(this)));

#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback("requestCrashUpload",
      base::Bind(&CrashesDOMHandler::HandleRequestUploads,
                 base::Unretained(this)));
#endif
}

void CrashesDOMHandler::HandleRequestCrashes(const base::ListValue* args) {
  if (first_load_) {
    first_load_ = false;
    if (list_available_)
      UpdateUI();
  } else {
    list_available_ = false;
    upload_list_->LoadUploadListAsynchronously();
  }
}

#if defined(OS_CHROMEOS)
void CrashesDOMHandler::HandleRequestUploads(const base::ListValue* args) {
  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  DCHECK(debugd_client);

  debugd_client->UploadCrashes();
}
#endif

void CrashesDOMHandler::OnUploadListAvailable() {
  list_available_ = true;
  if (!first_load_)
    UpdateUI();
}

void CrashesDOMHandler::UpdateUI() {
  bool crash_reporting_enabled =
      ChromeMetricsServiceAccessor::IsCrashReportingEnabled();
  base::ListValue crash_list;
  bool system_crash_reporter = false;

#if defined(OS_CHROMEOS)
  // Chrome OS has a system crash reporter.
  system_crash_reporter = true;
#endif

  if (crash_reporting_enabled) {
    std::vector<CrashUploadList::UploadInfo> crashes;
    upload_list_->GetUploads(50, &crashes);

    for (std::vector<CrashUploadList::UploadInfo>::iterator i = crashes.begin();
         i != crashes.end(); ++i) {
      base::DictionaryValue* crash = new base::DictionaryValue();
      crash->SetString("id", i->id);
      crash->SetString("time", base::TimeFormatFriendlyDateAndTime(i->time));
      crash->SetString("local_id", i->local_id);
      crash_list.Append(crash);
    }
  }

  base::FundamentalValue enabled(crash_reporting_enabled);
  base::FundamentalValue dynamic_backend(system_crash_reporter);

  const chrome::VersionInfo version_info;
  base::StringValue version(version_info.Version());

  web_ui()->CallJavascriptFunction("updateCrashList", enabled, dynamic_backend,
                                   crash_list, version);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CrashesUI
//
///////////////////////////////////////////////////////////////////////////////

CrashesUI::CrashesUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new CrashesDOMHandler());

  // Set up the chrome://crashes/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateCrashesUIHTMLSource());
}

// static
base::RefCountedMemory* CrashesUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_SAD_FAVICON, scale_factor);
}
