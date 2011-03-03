// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gpu_internals_ui.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gpu_data_manager.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/connection_tester.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu_process_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class GpuHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  GpuHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  ~GpuHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(GpuHTMLSource);
};

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
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // Mesages
  void OnBrowserBridgeInitialized(const ListValue* list);
  void OnCallAsync(const ListValue* list);

  // Submessages dispatched from OnCallAsync
  Value* OnRequestClientInfo(const ListValue* list);
  Value* OnRequestLogMessages(const ListValue* list);

  void OnGpuInfoUpdate();

  // Executes the javascript function |function_name| in the renderer, passing
  // it the argument |value|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value* value);

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);

  // Cache the Singleton for efficiency.
  GpuDataManager* gpu_data_manager_;

  Callback0::Type* gpu_info_update_callback_;
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

GpuHTMLSource::GpuHTMLSource()
    : DataSource(chrome::kChromeUIGpuInternalsHost, MessageLoop::current()) {
}

void GpuHTMLSource::StartDataRequest(const std::string& path,
    bool is_off_the_record,
    int request_id) {
  DictionaryValue localized_strings;
  SetFontAndTextDirection(&localized_strings);

  base::StringPiece gpu_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_GPU_INTERNALS_HTML));
  std::string full_html(gpu_html.data(), gpu_html.size());
  jstemplate_builder::AppendJsonHtml(&localized_strings, &full_html);
  jstemplate_builder::AppendI18nTemplateSourceHtml(&full_html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);


  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string GpuHTMLSource::GetMimeType(const std::string&) const {
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler() : gpu_info_update_callback_(NULL) {
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

  web_ui_->RegisterMessageCallback(
      "browserBridgeInitialized",
      NewCallback(this, &GpuMessageHandler::OnBrowserBridgeInitialized));
  web_ui_->RegisterMessageCallback(
      "callAsync",
      NewCallback(this, &GpuMessageHandler::OnCallAsync));
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
    web_ui_->CallJavascriptFunction(L"browserBridge.onCallAsyncReply",
        *requestId,
        *ret);
    delete ret;
  } else {
    web_ui_->CallJavascriptFunction(L"browserBridge.onCallAsyncReply",
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
      platform_util::GetVersionStringModifier());
    dict->SetString("official",
      l10n_util::GetStringUTF16(
      version_info.IsOfficialBuild() ?
      IDS_ABOUT_VERSION_OFFICIAL
      : IDS_ABOUT_VERSION_UNOFFICIAL));

    dict->SetString("command_line",
      CommandLine::ForCurrentProcess()->command_line_string());
  }

  return dict;
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    const std::string& value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    Value* value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->Set("value", value);
  return dict;
}

#if defined(OS_WIN)
// Output DxDiagNode tree as nested array of {description,value} pairs
ListValue* DxDiagNodeToList(const DxDiagNode& node) {
  ListValue* list = new ListValue();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    ListValue* sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, sublist));
  }
  return list;
}

#endif  // OS_WIN

std::string VersionNumberToString(uint32 value) {
  int hi = (value >> 8) & 0xff;
  int low = value & 0xff;
  return base::IntToString(hi) + "." + base::IntToString(low);
}

DictionaryValue* GpuInfoToDict(const GPUInfo& gpu_info) {
  ListValue* basic_info = new ListValue();
  basic_info->Append(NewDescriptionValuePair("Initialization time",
      base::Int64ToString(gpu_info.initialization_time().InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair("Vendor Id",
      base::StringPrintf("0x%04x", gpu_info.vendor_id())));
  basic_info->Append(NewDescriptionValuePair("Device Id",
      base::StringPrintf("0x%04x", gpu_info.device_id())));
  basic_info->Append(NewDescriptionValuePair("Driver vendor",
      gpu_info.driver_vendor()));
  basic_info->Append(NewDescriptionValuePair("Driver version",
      gpu_info.driver_version()));
  basic_info->Append(NewDescriptionValuePair("Driver date",
      gpu_info.driver_date()));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
      VersionNumberToString(gpu_info.pixel_shader_version())));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
      VersionNumberToString(gpu_info.vertex_shader_version())));
  basic_info->Append(NewDescriptionValuePair("GL version",
      VersionNumberToString(gpu_info.gl_version())));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
      gpu_info.gl_vendor()));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
      gpu_info.gl_renderer()));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
      gpu_info.gl_version_string()));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
      gpu_info.gl_extensions()));

  DictionaryValue* info = new DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  if (gpu_info.level() == GPUInfo::kComplete) {
    ListValue* dx_info = DxDiagNodeToList(gpu_info.dx_diagnostics());
    info->Set("diagnostics", dx_info);
  }
#endif

  return info;
}

Value* GpuMessageHandler::OnRequestLogMessages(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return gpu_data_manager_->log_messages().DeepCopy();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  const GPUInfo& gpu_info = gpu_data_manager_->gpu_info();
  if (gpu_info.level() == GPUInfo::kUninitialized)
    return;

  // Get GPU Info.
  DictionaryValue* gpu_info_val = GpuInfoToDict(gpu_info);

  // Add in blacklisting reasons
  Value* blacklisting_reasons = gpu_data_manager_->GetBlacklistingReasons();
  if (blacklisting_reasons)
    gpu_info_val->Set("blacklistingReasons", blacklisting_reasons);


  // Send GPU Info to javascript.
  web_ui_->CallJavascriptFunction(L"browserBridge.onGpuInfoUpdate",
      *gpu_info_val);

  delete gpu_info_val;
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(TabContents* contents) : WebUI(contents) {
  AddMessageHandler((new GpuMessageHandler())->Attach(this));

  GpuHTMLSource* html_source = new GpuHTMLSource();

  // Set up the chrome://gpu/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

