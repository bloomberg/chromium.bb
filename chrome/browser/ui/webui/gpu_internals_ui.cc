// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gpu_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_old.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "third_party/angle/src/common/version.h"
#include "ui/base/l10n/l10n_util.h"

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
      public base::SupportsWeakPtr<GpuMessageHandler> {
 public:
  GpuMessageHandler();
  virtual ~GpuMessageHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // Messages
  void OnBrowserBridgeInitialized(const ListValue* list);
  void OnCallAsync(const ListValue* list);

  // Submessages dispatched from OnCallAsync
  Value* OnRequestClientInfo(const ListValue* list);
  Value* OnRequestLogMessages(const ListValue* list);

  // Callbacks.
  void OnGpuInfoUpdate();

  // Executes the javascript function |function_name| in the renderer, passing
  // it the argument |value|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value* value);

 private:
  // Cache the Singleton for efficiency.
  GpuDataManager* gpu_data_manager_;

  Callback0::Type* gpu_info_update_callback_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler()
  : gpu_info_update_callback_(NULL) {
  gpu_data_manager_ = GpuDataManager::GetInstance();
  DCHECK(gpu_data_manager_);
}

GpuMessageHandler::~GpuMessageHandler() {
  if (gpu_info_update_callback_) {
    gpu_data_manager_->RemoveGpuInfoUpdateCallback(gpu_info_update_callback_);
    delete gpu_info_update_callback_;
  }
}

WebUIMessageHandler* GpuMessageHandler::Attach(WebUI* web_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);
  return result;
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui_->RegisterMessageCallback("browserBridgeInitialized",
      base::Bind(&GpuMessageHandler::OnBrowserBridgeInitialized,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("callAsync",
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
  } else {  // unrecognized submessage
    NOTREACHED();
    delete submessageArgs;
    return;
  }
  delete submessageArgs;

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui_->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId,
        *ret);
    delete ret;
  } else {
    web_ui_->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(!gpu_info_update_callback_);

  // Watch for changes in GPUInfo
  gpu_info_update_callback_ =
      NewCallback(this, &GpuMessageHandler::OnGpuInfoUpdate);
  gpu_data_manager_->AddGpuInfoUpdateCallback(gpu_info_update_callback_);

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  gpu_data_manager_->RequestCompleteGpuInfoIfNeeded();

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
  dict->SetString("blacklist_version",
      GpuDataManager::GetInstance()->GetBlacklistVersion());

  return dict;
}

Value* GpuMessageHandler::OnRequestLogMessages(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return gpu_data_manager_->log_messages().DeepCopy();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  scoped_ptr<base::DictionaryValue> gpu_info_val(
      gpu_data_manager_->GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  Value* feature_status = gpu_data_manager_->GetFeatureStatus();
  if (feature_status)
    gpu_info_val->Set("featureStatus", feature_status);

  // Send GPU Info to javascript.
  web_ui_->CallJavascriptFunction("browserBridge.onGpuInfoUpdate",
      *(gpu_info_val.get()));
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(TabContents* contents) : ChromeWebUI(contents) {
  AddMessageHandler((new GpuMessageHandler())->Attach(this));

  // Set up the chrome://gpu-internals/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(CreateGpuHTMLSource());
}
