// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/crashes_ui.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/crash/core/browser/crashes_ui_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/components_chromium_strings.h"
#include "grit/components_google_chrome_strings.h"
#include "grit/components_resources.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
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

  for (size_t i = 0; i < crash::kCrashesUILocalizedStringsCount; ++i) {
    source->AddLocalizedString(
        crash::kCrashesUILocalizedStrings[i].name,
        crash::kCrashesUILocalizedStrings[i].resource_id);
  }

  source->AddLocalizedString(crash::kCrashesUIShortProductName,
                             IDS_SHORT_PRODUCT_NAME);
  source->SetJsonPath("strings.js");
  source->AddResourcePath(crash::kCrashesUICrashesJS, IDR_CRASH_CRASHES_JS);
  source->SetDefaultResource(IDR_CRASH_CRASHES_HTML);
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
  ~CrashesDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // CrashUploadList::Delegate implemenation.
  void OnUploadListAvailable() override;

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
  upload_list_ = CreateCrashUploadList(this);
}

CrashesDOMHandler::~CrashesDOMHandler() {
  upload_list_->ClearDelegate();
}

void CrashesDOMHandler::RegisterMessages() {
  upload_list_->LoadUploadListAsynchronously();
  web_ui()->RegisterMessageCallback(
      crash::kCrashesUIRequestCrashList,
      base::Bind(&CrashesDOMHandler::HandleRequestCrashes,
                 base::Unretained(this)));

#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      crash::kCrashesUIRequestCrashUpload,
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
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  bool system_crash_reporter = false;
#if defined(OS_CHROMEOS)
  // Chrome OS has a system crash reporter.
  system_crash_reporter = true;
#endif

  base::ListValue crash_list;
  if (crash_reporting_enabled)
    crash::UploadListToValue(upload_list_.get(), &crash_list);

  base::FundamentalValue enabled(crash_reporting_enabled);
  base::FundamentalValue dynamic_backend(system_crash_reporter);
  base::StringValue version(version_info::GetVersionNumber());
  base::StringValue os_string(base::SysInfo::OperatingSystemName() + " " +
                              base::SysInfo::OperatingSystemVersion());

  std::vector<const base::Value*> args;
  args.push_back(&enabled);
  args.push_back(&dynamic_backend);
  args.push_back(&crash_list);
  args.push_back(&version);
  args.push_back(&os_string);
  web_ui()->CallJavascriptFunction(crash::kCrashesUIUpdateCrashList, args);
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
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_CRASH_SAD_FAVICON, scale_factor);
}
