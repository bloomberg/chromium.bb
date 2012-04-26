// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gpu_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/gpu_blacklist.h"
#include "chrome/browser/gpu_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/crashes_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "third_party/angle/src/common/version.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::GpuDataManager;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateGpuHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIGpuInternalsHost);

  source->set_json_path("strings.js");
  source->add_resource_path("gpu_internals.js", IDR_GPU_INTERNALS_JS);
  source->set_default_resource(IDR_GPU_INTERNALS_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class GpuMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public content::GpuDataManagerObserver,
      public CrashUploadList::Delegate {
 public:
  GpuMessageHandler();
  virtual ~GpuMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // GpuDataManagerObserver implementation.
  virtual void OnGpuInfoUpdate() OVERRIDE;

  // CrashUploadList::Delegate implemenation.
  virtual void OnCrashListAvailable() OVERRIDE;

  // Messages
  void OnBrowserBridgeInitialized(const ListValue* list);
  void OnCallAsync(const ListValue* list);

  // Submessages dispatched from OnCallAsync
  Value* OnRequestClientInfo(const ListValue* list);
  Value* OnRequestLogMessages(const ListValue* list);
  Value* OnRequestCrashList(const ListValue* list);

  // Executes the javascript function |function_name| in the renderer, passing
  // it the argument |value|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value* value);

 private:
  scoped_refptr<CrashUploadList> crash_list_;
  bool crash_list_available_;

  // True if observing the GpuDataManager (re-attaching as observer would
  // DCHECK).
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler()
    : crash_list_available_(false),
      observing_(false) {
  crash_list_ = CrashUploadList::Create(this);
}

GpuMessageHandler::~GpuMessageHandler() {
  GpuDataManager::GetInstance()->RemoveObserver(this);
  crash_list_->ClearDelegate();
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  crash_list_->LoadCrashListAsynchronously();

  web_ui()->RegisterMessageCallback("browserBridgeInitialized",
      base::Bind(&GpuMessageHandler::OnBrowserBridgeInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("callAsync",
      base::Bind(&GpuMessageHandler::OnCallAsync,
                 base::Unretained(this)));
}

void GpuMessageHandler::OnCallAsync(const ListValue* args) {
  DCHECK_GE(args->GetSize(), static_cast<size_t>(2));
  // unpack args into requestId, submessage and submessageArgs
  bool ok;
  Value* requestId;
  ok = args->Get(0, &requestId);
  DCHECK(ok);

  std::string submessage;
  ok = args->GetString(1, &submessage);
  DCHECK(ok);

  ListValue* submessageArgs = new ListValue();
  for (size_t i = 2; i < args->GetSize(); ++i) {
    Value* arg;
    ok = args->Get(i, &arg);
    DCHECK(ok);

    Value* argCopy = arg->DeepCopy();
    submessageArgs->Append(argCopy);
  }

  // call the submessage handler
  Value* ret = NULL;
  if (submessage == "requestClientInfo") {
    ret = OnRequestClientInfo(submessageArgs);
  } else if (submessage == "requestLogMessages") {
    ret = OnRequestLogMessages(submessageArgs);
  } else if (submessage == "requestCrashList") {
    ret = OnRequestCrashList(submessageArgs);
  } else {  // unrecognized submessage
    NOTREACHED();
    delete submessageArgs;
    return;
  }
  delete submessageArgs;

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui()->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId,
        *ret);
    delete ret;
  } else {
    web_ui()->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Watch for changes in GPUInfo
  if (!observing_)
    GpuDataManager::GetInstance()->AddObserver(this);
  observing_ = true;

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  GpuDataManager::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();
}

Value* GpuMessageHandler::OnRequestClientInfo(const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DictionaryValue* dict = new DictionaryValue();

  chrome::VersionInfo version_info;

  if (!version_info.is_valid()) {
    DLOG(ERROR) << "Unable to create chrome::VersionInfo";
  } else {
    // We have everything we need to send the right values.
    dict->SetString("version", version_info.Version());
    dict->SetString("cl", version_info.LastChange());
    dict->SetString("version_mod",
        chrome::VersionInfo::GetVersionStringModifier());
    dict->SetString("official",
        l10n_util::GetStringUTF16(
            version_info.IsOfficialBuild() ?
            IDS_ABOUT_VERSION_OFFICIAL :
            IDS_ABOUT_VERSION_UNOFFICIAL));

    dict->SetString("command_line",
        CommandLine::ForCurrentProcess()->GetCommandLineString());
  }

  dict->SetString("operating_system",
                  base::SysInfo::OperatingSystemName() + " " +
                  base::SysInfo::OperatingSystemVersion());
  dict->SetString("angle_revision", base::UintToString(BUILD_REVISION));
#if defined(USE_SKIA)
  dict->SetString("graphics_backend", "Skia");
#else
  dict->SetString("graphics_backend", "Core Graphics");
#endif
  dict->SetString("blacklist_version",
      GpuBlacklist::GetInstance()->GetVersion());

  return dict;
}

Value* GpuMessageHandler::OnRequestLogMessages(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return GpuDataManager::GetInstance()->GetLogMessages().DeepCopy();
}

Value* GpuMessageHandler::OnRequestCrashList(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!CrashesUI::CrashReportingEnabled()) {
    // We need to return an empty list instead of NULL.
    return new ListValue;
  }
  if (!crash_list_available_) {
    // If we are still obtaining crash list, then return null so another
    // request will be scheduled.
    return NULL;
  }

  ListValue* list_value = new ListValue;
  std::vector<CrashUploadList::CrashInfo> crashes;
  crash_list_->GetUploadedCrashes(50, &crashes);
  for (std::vector<CrashUploadList::CrashInfo>::iterator i = crashes.begin();
       i != crashes.end(); ++i) {
    DictionaryValue* crash = new DictionaryValue();
    crash->SetString("id", i->crash_id);
    crash->SetString("time",
                     base::TimeFormatFriendlyDateAndTime(i->crash_time));
    list_value->Append(crash);
  }
  return list_value;
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  scoped_ptr<base::DictionaryValue> gpu_info_val(
      gpu_util::GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  Value* feature_status = gpu_util::GetFeatureStatus();
  if (feature_status)
    gpu_info_val->Set("featureStatus", feature_status);

  // Send GPU Info to javascript.
  web_ui()->CallJavascriptFunction("browserBridge.onGpuInfoUpdate",
      *(gpu_info_val.get()));
}

void GpuMessageHandler::OnCrashListAvailable() {
  crash_list_available_ = true;
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new GpuMessageHandler());

  // Set up the chrome://gpu-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateGpuHTMLSource());
}
