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
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
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
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/trace_controller.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class GpuHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  GpuHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
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
      public SelectFileDialog::Listener,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public TraceSubscriber {
 public:
  GpuMessageHandler();
  virtual ~GpuMessageHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // Mesages
  void OnBeginTracing(const ListValue* list);
  void OnEndTracingAsync(const ListValue* list);
  void OnBrowserBridgeInitialized(const ListValue* list);
  void OnCallAsync(const ListValue* list);
  void OnBeginRequestBufferPercentFull(const ListValue* list);
  void OnLoadTraceFile(const ListValue* list);
  void OnSaveTraceFile(const ListValue* list);

  // Submessages dispatched from OnCallAsync
  Value* OnRequestClientInfo(const ListValue* list);
  Value* OnRequestLogMessages(const ListValue* list);

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // Callbacks.
  void OnGpuInfoUpdate();
  void LoadTraceFileComplete(std::string* file_contents);
  void SaveTraceFileComplete();

  // TraceSubscriber implementation.
  virtual void OnEndTracingComplete();
  virtual void OnTraceDataCollected(const std::string& json_events);
  virtual void OnTraceBufferPercentFullReply(float percent_full);

  // Executes the javascript function |function_name| in the renderer, passing
  // it the argument |value|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value* value);

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);

  // Cache the Singleton for efficiency.
  GpuDataManager* gpu_data_manager_;

  Callback0::Type* gpu_info_update_callback_;

  scoped_refptr<SelectFileDialog> select_trace_file_dialog_;
  SelectFileDialog::Type select_trace_file_dialog_type_;
  scoped_ptr<std::string> trace_data_to_save_;

  bool trace_enabled_;
};

class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
 public:
  explicit TaskProxy(const base::WeakPtr<GpuMessageHandler>& handler)
      : handler_(handler) {}
  void LoadTraceFileCompleteProxy(std::string* file_contents) {
    if (handler_)
      handler_->LoadTraceFileComplete(file_contents);
    delete file_contents;
  }

  void SaveTraceFileCompleteProxy() {
    if (handler_)
      handler_->SaveTraceFileComplete();
  }

 private:
  base::WeakPtr<GpuMessageHandler> handler_;
  friend class base::RefCountedThreadSafe<TaskProxy>;
  DISALLOW_COPY_AND_ASSIGN(TaskProxy);
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
                                     bool is_incognito,
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

GpuMessageHandler::GpuMessageHandler()
  : gpu_info_update_callback_(NULL)
  , trace_enabled_(false) {
  gpu_data_manager_ = GpuDataManager::GetInstance();
  DCHECK(gpu_data_manager_);
}

GpuMessageHandler::~GpuMessageHandler() {
  if (gpu_info_update_callback_) {
    gpu_data_manager_->RemoveGpuInfoUpdateCallback(gpu_info_update_callback_);
    delete gpu_info_update_callback_;
  }

  if (select_trace_file_dialog_)
    select_trace_file_dialog_->ListenerDestroyed();

  // If we are the current subscriber, this will result in ending tracing.
  TraceController::GetInstance()->CancelSubscriber(this);
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
      "beginTracing",
      NewCallback(this, &GpuMessageHandler::OnBeginTracing));
  web_ui_->RegisterMessageCallback(
      "endTracingAsync",
      NewCallback(this, &GpuMessageHandler::OnEndTracingAsync));
  web_ui_->RegisterMessageCallback(
      "browserBridgeInitialized",
      NewCallback(this, &GpuMessageHandler::OnBrowserBridgeInitialized));
  web_ui_->RegisterMessageCallback(
      "callAsync",
      NewCallback(this, &GpuMessageHandler::OnCallAsync));
  web_ui_->RegisterMessageCallback(
      "beginRequestBufferPercentFull",
      NewCallback(this, &GpuMessageHandler::OnBeginRequestBufferPercentFull));
  web_ui_->RegisterMessageCallback(
      "loadTraceFile",
      NewCallback(this, &GpuMessageHandler::OnLoadTraceFile));
  web_ui_->RegisterMessageCallback(
      "saveTraceFile",
      NewCallback(this, &GpuMessageHandler::OnSaveTraceFile));
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

void GpuMessageHandler::OnBeginRequestBufferPercentFull(const ListValue* list) {
  TraceController::GetInstance()->GetTraceBufferPercentFullAsync(this);
}

class ReadTraceFileTask : public Task {
 public:
  ReadTraceFileTask(TaskProxy* proxy, const FilePath& path)
      : proxy_(proxy)
      , path_(path) {}

  virtual void Run() {
    std::string* file_contents = new std::string();
    if (!file_util::ReadFileToString(path_, file_contents))
      return;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(proxy_.get(),
                          &TaskProxy::LoadTraceFileCompleteProxy,
                          file_contents));
  }

 private:
  scoped_refptr<TaskProxy> proxy_;

  // Path of the file to open.
  const FilePath path_;
};

class WriteTraceFileTask : public Task {
 public:
  WriteTraceFileTask(TaskProxy* proxy,
                     const FilePath& path,
                     std::string* contents)
      : proxy_(proxy)
      , path_(path)
      , contents_(contents) {}

  virtual void Run() {
    if (!file_util::WriteFile(path_, contents_->c_str(), contents_->size()))
      return;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(proxy_.get(),
                          &TaskProxy::SaveTraceFileCompleteProxy));
  }

 private:
  scoped_refptr<TaskProxy> proxy_;

  // Path of the file to save.
  const FilePath path_;

  // What to save
  scoped_ptr<std::string> contents_;
};

void GpuMessageHandler::FileSelected(
    const FilePath& path, int index, void* params) {
  if(select_trace_file_dialog_type_ == SelectFileDialog::SELECT_OPEN_FILE)
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        new ReadTraceFileTask(new TaskProxy(AsWeakPtr()), path));
  else
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        new WriteTraceFileTask(new TaskProxy(AsWeakPtr()), path,
                               trace_data_to_save_.release()));
  select_trace_file_dialog_.release();
}

void GpuMessageHandler::FileSelectionCanceled(void* params) {
  select_trace_file_dialog_.release();
  if(select_trace_file_dialog_type_ == SelectFileDialog::SELECT_OPEN_FILE)
    web_ui_->CallJavascriptFunction("tracingController.onLoadTraceFileCanceled");
  else
    web_ui_->CallJavascriptFunction("tracingController.onSaveTraceFileCanceled");
}

void GpuMessageHandler::OnLoadTraceFile(const ListValue* list) {
  // Only allow a single dialog at a time.
  if (select_trace_file_dialog_.get())
    return;
  select_trace_file_dialog_type_ = SelectFileDialog::SELECT_OPEN_FILE;
  select_trace_file_dialog_ = SelectFileDialog::Create(this);
  select_trace_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_OPEN_FILE,
      string16(),
      FilePath(),
      NULL, 0, FILE_PATH_LITERAL(""), web_ui_->tab_contents(),
      web_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), NULL);
}

void GpuMessageHandler::LoadTraceFileComplete(std::string* file_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::wstring javascript;
  javascript += L"tracingController.onLoadTraceFileComplete(";
  javascript += UTF8ToWide(*file_contents);
  javascript += L");";

  web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(string16(),
      WideToUTF16Hack(javascript));
}

void GpuMessageHandler::OnSaveTraceFile(const ListValue* list) {
  // Only allow a single dialog at a time.
  if (select_trace_file_dialog_.get())
    return;

  DCHECK(list->GetSize() == 1);

  Value* tmp;
  list->Get(0, &tmp);

  std::string* trace_data = new std::string();
  bool ok = list->GetString(0, trace_data);
  DCHECK(ok);
  trace_data_to_save_.reset(trace_data);

  select_trace_file_dialog_type_ = SelectFileDialog::SELECT_SAVEAS_FILE;
  select_trace_file_dialog_ = SelectFileDialog::Create(this);
  select_trace_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE,
      string16(),
      FilePath(),
      NULL, 0, FILE_PATH_LITERAL(""), web_ui_->tab_contents(),
      web_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), NULL);
}

void GpuMessageHandler::SaveTraceFileComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::wstring javascript;
  web_ui_->CallJavascriptFunction("tracingController.onSaveTraceFileComplete");
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
            IDS_ABOUT_VERSION_OFFICIAL :
            IDS_ABOUT_VERSION_UNOFFICIAL));

    dict->SetString("command_line",
        CommandLine::ForCurrentProcess()->command_line_string());
  }

  dict->SetString("blacklist_version",
      GpuDataManager::GetInstance()->GetBlacklistVersion());

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

DictionaryValue* GpuInfoToDict(const GPUInfo& gpu_info) {
  ListValue* basic_info = new ListValue();
  basic_info->Append(NewDescriptionValuePair("Initialization time",
      base::Int64ToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair("Vendor Id",
      base::StringPrintf("0x%04x", gpu_info.vendor_id)));
  basic_info->Append(NewDescriptionValuePair("Device Id",
      base::StringPrintf("0x%04x", gpu_info.device_id)));
  basic_info->Append(NewDescriptionValuePair("Driver vendor",
      gpu_info.driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
      gpu_info.driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
      gpu_info.driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
      gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
      gpu_info.vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("GL version",
      gpu_info.gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
      gpu_info.gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
      gpu_info.gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
      gpu_info.gl_version_string));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
      gpu_info.gl_extensions));

  DictionaryValue* info = new DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  Value* dx_info;
  if (gpu_info.dx_diagnostics.children.size())
    dx_info = DxDiagNodeToList(gpu_info.dx_diagnostics);
  else
    dx_info = Value::CreateNullValue();
  info->Set("diagnostics", dx_info);
#endif

  return info;
}

Value* GpuMessageHandler::OnRequestLogMessages(const ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return gpu_data_manager_->log_messages().DeepCopy();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  const GPUInfo& gpu_info = gpu_data_manager_->gpu_info();

  // Get GPU Info.
  DictionaryValue* gpu_info_val = GpuInfoToDict(gpu_info);

  // Add in blacklisting features
  Value* feature_status = gpu_data_manager_->GetFeatureStatus();
  if (feature_status)
    gpu_info_val->Set("featureStatus", feature_status);

  // Send GPU Info to javascript.
  web_ui_->CallJavascriptFunction("browserBridge.onGpuInfoUpdate",
      *gpu_info_val);

  delete gpu_info_val;
}

void GpuMessageHandler::OnBeginTracing(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  trace_enabled_ = true;
  // TODO(jbates) This may fail, but that's OK for current use cases.
  //              Ex: Multiple about:gpu traces can not trace simultaneously.
  // TODO(nduca) send feedback to javascript about whether or not BeginTracing
  //             was successful.
  TraceController::GetInstance()->BeginTracing(this);
}

void GpuMessageHandler::OnEndTracingAsync(const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(nduca): fix javascript code to make sure trace_enabled_ is always true
  //              here. triggered a false condition by just clicking stop
  //              trace a few times when it was going slow, and maybe switching
  //              between tabs.
  if (trace_enabled_ &&
      !TraceController::GetInstance()->EndTracingAsync(this)) {
    // Set to false now, since it turns out we never were the trace subscriber.
    OnEndTracingComplete();
  }
}

void GpuMessageHandler::OnEndTracingComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  trace_enabled_ = false;
  web_ui_->CallJavascriptFunction("tracingController.onEndTracingComplete");
}

void GpuMessageHandler::OnTraceDataCollected(const std::string& json_events) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::wstring javascript;
  javascript += L"tracingController.onTraceDataCollected(";
  javascript += UTF8ToWide(json_events);
  javascript += L");";

  web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(string16(),
      WideToUTF16Hack(javascript));
}

void GpuMessageHandler::OnTraceBufferPercentFullReply(float percent_full) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui_->CallJavascriptFunction(
      "tracingController.onRequestBufferPercentFullComplete",
      *scoped_ptr<Value>(Value::CreateDoubleValue(percent_full)));
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
