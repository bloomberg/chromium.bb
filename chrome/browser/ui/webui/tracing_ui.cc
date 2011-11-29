// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tracing_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/trace_controller.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

ChromeWebUIDataSource* CreateTracingHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUITracingHost);

  source->set_json_path("strings.js");
  source->add_resource_path("tracing.js", IDR_TRACING_JS);
  source->set_default_resource(IDR_TRACING_HTML);
  source->AddLocalizedString("tracingTitle", IDS_TRACING_TITLE);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class TracingMessageHandler
    : public WebUIMessageHandler,
      public SelectFileDialog::Listener,
      public base::SupportsWeakPtr<TracingMessageHandler>,
      public TraceSubscriber,
      public GpuDataManager::Observer {
 public:
  TracingMessageHandler();
  virtual ~TracingMessageHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // TraceSubscriber implementation.
  virtual void OnEndTracingComplete();
  virtual void OnTraceDataCollected(const std::string& trace_fragment);
  virtual void OnTraceBufferPercentFullReply(float percent_full);

  // GpuDataManager::Observer implementation.
  virtual void OnGpuInfoUpdate() OVERRIDE;

  // Messages.
  void OnTracingControllerInitialized(const ListValue* list);
  void OnBeginTracing(const ListValue* list);
  void OnEndTracingAsync(const ListValue* list);
  void OnBeginRequestBufferPercentFull(const ListValue* list);
  void OnLoadTraceFile(const ListValue* list);
  void OnSaveTraceFile(const ListValue* list);

  // Callbacks.
  void LoadTraceFileComplete(std::string* file_contents);
  void SaveTraceFileComplete();

 private:
  // The file dialog to select a file for loading or saving traces.
  scoped_refptr<SelectFileDialog> select_trace_file_dialog_;

  // The type of the file dialog as the same one is used for loading or saving
  // traces.
  SelectFileDialog::Type select_trace_file_dialog_type_;

  // The trace data that is to be written to the file on saving.
  scoped_ptr<std::string> trace_data_to_save_;

  // True while tracing is active.
  bool trace_enabled_;

  // Cache the Singleton for efficiency.
  GpuDataManager* gpu_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(TracingMessageHandler);
};

// A proxy passed to the Read and Write tasks used when loading or saving trace
// data.
class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
 public:
  explicit TaskProxy(const base::WeakPtr<TracingMessageHandler>& handler)
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
  friend class base::RefCountedThreadSafe<TaskProxy>;

  // The message handler to call callbacks on.
  base::WeakPtr<TracingMessageHandler> handler_;

  DISALLOW_COPY_AND_ASSIGN(TaskProxy);
};

////////////////////////////////////////////////////////////////////////////////
//
// TracingMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

TracingMessageHandler::TracingMessageHandler()
  : select_trace_file_dialog_type_(SelectFileDialog::SELECT_NONE),
    trace_enabled_(false) {
  gpu_data_manager_ = GpuDataManager::GetInstance();
  DCHECK(gpu_data_manager_);
}

TracingMessageHandler::~TracingMessageHandler() {
  gpu_data_manager_->RemoveObserver(this);

  if (select_trace_file_dialog_)
    select_trace_file_dialog_->ListenerDestroyed();

  // If we are the current subscriber, this will result in ending tracing.
  TraceController::GetInstance()->CancelSubscriber(this);
}

WebUIMessageHandler* TracingMessageHandler::Attach(WebUI* web_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);
  return result;
}

void TracingMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui_->RegisterMessageCallback("tracingControllerInitialized",
      base::Bind(&TracingMessageHandler::OnTracingControllerInitialized,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("beginTracing",
      base::Bind(&TracingMessageHandler::OnBeginTracing,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("endTracingAsync",
      base::Bind(&TracingMessageHandler::OnEndTracingAsync,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("beginRequestBufferPercentFull",
      base::Bind(&TracingMessageHandler::OnBeginRequestBufferPercentFull,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("loadTraceFile",
      base::Bind(&TracingMessageHandler::OnLoadTraceFile,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("saveTraceFile",
      base::Bind(&TracingMessageHandler::OnSaveTraceFile,
                 base::Unretained(this)));
}

void TracingMessageHandler::OnTracingControllerInitialized(
    const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Watch for changes in GPUInfo
  gpu_data_manager_->AddObserver(this);

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  gpu_data_manager_->RequestCompleteGpuInfoIfNeeded();

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();

  // Send the client info to the tracingController
  {
    scoped_ptr<DictionaryValue> dict(new DictionaryValue());
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

    dict->SetString("blacklist_version",
        GpuDataManager::GetInstance()->GetBlacklistVersion());
    web_ui_->CallJavascriptFunction("tracingController.onClientInfoUpdate",
                                    *dict);
  }
}

void TracingMessageHandler::OnBeginRequestBufferPercentFull(
    const ListValue* list) {
  TraceController::GetInstance()->GetTraceBufferPercentFullAsync(this);
}

void TracingMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  scoped_ptr<base::DictionaryValue> gpu_info_val(
      gpu_data_manager_->GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  Value* feature_status = gpu_data_manager_->GetFeatureStatus();
  if (feature_status)
    gpu_info_val->Set("featureStatus", feature_status);

  // Send GPU Info to javascript.
  web_ui_->CallJavascriptFunction("tracingController.onGpuInfoUpdate",
      *(gpu_info_val.get()));
}

// A callback used for asynchronously reading a file to a string. Calls the
// TaskProxy callback when reading is complete.
void ReadTraceFileCallback(TaskProxy* proxy, const FilePath& path) {
  scoped_ptr<std::string> file_contents(new std::string());
  if (!file_util::ReadFileToString(path, file_contents.get()))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskProxy::LoadTraceFileCompleteProxy, proxy,
                 file_contents.release()));
}

// A callback used for asynchronously writing a file from a string. Calls the
// TaskProxy callback when writing is complete.
void WriteTraceFileCallback(TaskProxy* proxy,
                            const FilePath& path,
                            std::string* contents) {
  if (!file_util::WriteFile(path, contents->c_str(), contents->size()))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskProxy::SaveTraceFileCompleteProxy, proxy));
}

void TracingMessageHandler::FileSelected(
    const FilePath& path, int index, void* params) {
  if (select_trace_file_dialog_type_ == SelectFileDialog::SELECT_OPEN_FILE) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ReadTraceFileCallback,
                   make_scoped_refptr(new TaskProxy(AsWeakPtr())), path));
  } else {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&WriteTraceFileCallback,
                   make_scoped_refptr(new TaskProxy(AsWeakPtr())), path,
                   trace_data_to_save_.release()));
  }

  select_trace_file_dialog_.release();
}

void TracingMessageHandler::FileSelectionCanceled(void* params) {
  select_trace_file_dialog_.release();
  if (select_trace_file_dialog_type_ == SelectFileDialog::SELECT_OPEN_FILE) {
    web_ui_->CallJavascriptFunction(
        "tracingController.onLoadTraceFileCanceled");
  } else {
    web_ui_->CallJavascriptFunction(
        "tracingController.onSaveTraceFileCanceled");
  }
}

void TracingMessageHandler::OnLoadTraceFile(const ListValue* list) {
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

void TracingMessageHandler::LoadTraceFileComplete(std::string* file_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string javascript = "tracingController.onLoadTraceFileComplete("
      + *file_contents + ");";

  web_ui_->tab_contents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(javascript));
}

void TracingMessageHandler::OnSaveTraceFile(const ListValue* list) {
  // Only allow a single dialog at a time.
  if (select_trace_file_dialog_.get())
    return;

  DCHECK(list->GetSize() == 1);

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

void TracingMessageHandler::SaveTraceFileComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui_->CallJavascriptFunction("tracingController.onSaveTraceFileComplete");
}

void TracingMessageHandler::OnBeginTracing(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  trace_enabled_ = true;
  // TODO(jbates) This may fail, but that's OK for current use cases.
  //              Ex: Multiple about:gpu traces can not trace simultaneously.
  // TODO(nduca) send feedback to javascript about whether or not BeginTracing
  //             was successful.
  TraceController::GetInstance()->BeginTracing(this);
}

void TracingMessageHandler::OnEndTracingAsync(const ListValue* list) {
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

void TracingMessageHandler::OnEndTracingComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  trace_enabled_ = false;
  web_ui_->CallJavascriptFunction("tracingController.onEndTracingComplete");
}

void TracingMessageHandler::OnTraceDataCollected(
    const std::string& trace_fragment) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::debug::TraceResultBuffer::SimpleOutput output;
  base::debug::TraceResultBuffer trace_buffer;
  trace_buffer.SetOutputCallback(output.GetCallback());
  output.Append("tracingController.onTraceDataCollected(");
  trace_buffer.Start();
  trace_buffer.AddFragment(trace_fragment);
  trace_buffer.Finish();
  output.Append(");");

  web_ui_->tab_contents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(output.json_output));
}

void TracingMessageHandler::OnTraceBufferPercentFullReply(float percent_full) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui_->CallJavascriptFunction(
      "tracingController.onRequestBufferPercentFullComplete",
      *scoped_ptr<Value>(Value::CreateDoubleValue(percent_full)));
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// TracingUI
//
////////////////////////////////////////////////////////////////////////////////

TracingUI::TracingUI(TabContents* contents) : ChromeWebUI(contents) {
  AddMessageHandler((new TracingMessageHandler())->Attach(this));

  // Set up the chrome://tracing/ source.
  Profile::FromBrowserContext(contents->browser_context())->
      GetChromeURLDataManager()->AddDataSource(CreateTracingHTMLSource());
}
