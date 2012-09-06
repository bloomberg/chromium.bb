// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tracing_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/trace_subscriber.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_channel.h"
#include "ui/base/dialogs/select_file_dialog.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#endif

using content::BrowserThread;
using content::GpuDataManager;
using content::TraceController;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateTracingHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUITracingHost);

  source->set_json_path("strings.js");
  source->set_default_resource(IDR_TRACING_HTML);
  source->add_resource_path("tracing.js", IDR_TRACING_JS);
  source->AddLocalizedString("tracingTitle", IDS_TRACING_TITLE);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class TracingMessageHandler
    : public WebUIMessageHandler,
      public ui::SelectFileDialog::Listener,
      public base::SupportsWeakPtr<TracingMessageHandler>,
      public content::TraceSubscriber {
 public:
  TracingMessageHandler();
  virtual ~TracingMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // SelectFileDialog::Listener implementation
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void FileSelectionCanceled(void* params);

  // TraceSubscriber implementation.
  virtual void OnEndTracingComplete();
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& trace_fragment);
  virtual void OnTraceBufferPercentFullReply(float percent_full);

  // Messages.
  void OnTracingControllerInitialized(const ListValue* list);
  void OnBeginTracing(const ListValue* list);
  void OnEndTracingAsync(const ListValue* list);
  void OnBeginRequestBufferPercentFull(const ListValue* list);
  void OnLoadTraceFile(const ListValue* list);
  void OnSaveTraceFile(const ListValue* list);

  // Callbacks.
  void LoadTraceFileComplete(string16* file_contents);
  void SaveTraceFileComplete();

 private:
  // The file dialog to select a file for loading or saving traces.
  scoped_refptr<ui::SelectFileDialog> select_trace_file_dialog_;

  // The type of the file dialog as the same one is used for loading or saving
  // traces.
  ui::SelectFileDialog::Type select_trace_file_dialog_type_;

  // The trace data that is to be written to the file on saving.
  scoped_ptr<std::string> trace_data_to_save_;

  // True while tracing is active.
  bool trace_enabled_;

  // True while system tracing is active.
  bool system_trace_in_progress_;

  void OnEndSystemTracingAck(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

  DISALLOW_COPY_AND_ASSIGN(TracingMessageHandler);
};

// A proxy passed to the Read and Write tasks used when loading or saving trace
// data.
class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
 public:
  explicit TaskProxy(const base::WeakPtr<TracingMessageHandler>& handler)
      : handler_(handler) {}
  void LoadTraceFileCompleteProxy(string16* file_contents) {
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
  ~TaskProxy() {}

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
    : select_trace_file_dialog_type_(ui::SelectFileDialog::SELECT_NONE),
      trace_enabled_(false),
      system_trace_in_progress_(false) {
}

TracingMessageHandler::~TracingMessageHandler() {
  if (select_trace_file_dialog_)
    select_trace_file_dialog_->ListenerDestroyed();

  // If we are the current subscriber, this will result in ending tracing.
  TraceController::GetInstance()->CancelSubscriber(this);

  // Shutdown any system tracing too.
    if (system_trace_in_progress_) {
#if defined(OS_CHROMEOS)
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
          RequestStopSystemTracing(
              chromeos::DebugDaemonClient::EmptyStopSystemTracingCallback());
#endif
    }
}

void TracingMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui()->RegisterMessageCallback("tracingControllerInitialized",
      base::Bind(&TracingMessageHandler::OnTracingControllerInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("beginTracing",
      base::Bind(&TracingMessageHandler::OnBeginTracing,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("endTracingAsync",
      base::Bind(&TracingMessageHandler::OnEndTracingAsync,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("beginRequestBufferPercentFull",
      base::Bind(&TracingMessageHandler::OnBeginRequestBufferPercentFull,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("loadTraceFile",
      base::Bind(&TracingMessageHandler::OnLoadTraceFile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveTraceFile",
      base::Bind(&TracingMessageHandler::OnSaveTraceFile,
                 base::Unretained(this)));
}

void TracingMessageHandler::OnTracingControllerInitialized(
    const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
    web_ui()->CallJavascriptFunction("tracingController.onClientInfoUpdate",
                                     *dict);
  }
}

void TracingMessageHandler::OnBeginRequestBufferPercentFull(
    const ListValue* list) {
  TraceController::GetInstance()->GetTraceBufferPercentFullAsync(this);
}

// A callback used for asynchronously reading a file to a string. Calls the
// TaskProxy callback when reading is complete.
void ReadTraceFileCallback(TaskProxy* proxy, const FilePath& path) {
  std::string file_contents;
  if (!file_util::ReadFileToString(path, &file_contents))
    return;

  // We need to escape the file contents, because it will go into a javascript
  // quoted string in TracingMessageHandler::LoadTraceFileComplete. We need to
  // escape control characters (to have well-formed javascript statements), as
  // well as \ and ' (the only special characters in a ''-quoted string).
  // Do the escaping on this thread, it may take a little while for big files
  // and we don't want to block the UI during that time. Also do the UTF-16
  // conversion here.
  // Note: we're using UTF-16 because we'll need to cut the string into slices
  // to give to Javascript, and it's easier to cut than UTF-8 (since JS strings
  // are arrays of 16-bit values, UCS-2 really, whereas we can't cut inside of a
  // multibyte UTF-8 codepoint).
  size_t size = file_contents.size();
  std::string escaped_contents;
  escaped_contents.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    char c = file_contents[i];
    if (c < ' ') {
      escaped_contents += base::StringPrintf("\\u%04x", c);
      continue;
    }
    if (c == '\\' || c == '\'')
      escaped_contents.push_back('\\');
    escaped_contents.push_back(c);
  }
  file_contents.clear();

  scoped_ptr<string16> contents16(new string16);
  UTF8ToUTF16(escaped_contents).swap(*contents16);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskProxy::LoadTraceFileCompleteProxy, proxy,
                 contents16.release()));
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
  if (select_trace_file_dialog_type_ ==
      ui::SelectFileDialog::SELECT_OPEN_FILE) {
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

  select_trace_file_dialog_ = NULL;
}

void TracingMessageHandler::FileSelectionCanceled(void* params) {
  select_trace_file_dialog_ = NULL;
  if (select_trace_file_dialog_type_ ==
      ui::SelectFileDialog::SELECT_OPEN_FILE) {
    web_ui()->CallJavascriptFunction(
        "tracingController.onLoadTraceFileCanceled");
  } else {
    web_ui()->CallJavascriptFunction(
        "tracingController.onSaveTraceFileCanceled");
  }
}

void TracingMessageHandler::OnLoadTraceFile(const ListValue* list) {
  // Only allow a single dialog at a time.
  if (select_trace_file_dialog_.get())
    return;
  select_trace_file_dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
  select_trace_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_trace_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      string16(),
      FilePath(),
      NULL, 0, FILE_PATH_LITERAL(""),
      web_ui()->GetWebContents()->GetView()->GetTopLevelNativeWindow(), NULL);
}

void TracingMessageHandler::LoadTraceFileComplete(string16* contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need to pass contents to tracingController.onLoadTraceFileComplete, but
  // that may be arbitrarily big, and IPCs messages are limited in size. So we
  // need to cut it into pieces and rebuild the string in Javascript.
  // |contents| has already been escaped in ReadTraceFileCallback.
  // IPC::Channel::kMaximumMessageSize is in bytes, and we need to account for
  // overhead.
  const size_t kMaxSize = IPC::Channel::kMaximumMessageSize / 2 - 128;
  string16 first_prefix = UTF8ToUTF16("window.traceData = '");
  string16 prefix = UTF8ToUTF16("window.traceData += '");
  string16 suffix = UTF8ToUTF16("';");

  content::RenderViewHost* rvh =
      web_ui()->GetWebContents()->GetRenderViewHost();
  for (size_t i = 0; i < contents->size(); i += kMaxSize) {
    string16 javascript = i == 0 ? first_prefix : prefix;
    javascript += contents->substr(i, kMaxSize) + suffix;
    rvh->ExecuteJavascriptInWebFrame(string16(), javascript);
  }
  rvh->ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(
      "tracingController.onLoadTraceFileComplete(JSON.parse(window.traceData));"
      "delete window.traceData;"));
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

  select_trace_file_dialog_type_ = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
  select_trace_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_trace_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      string16(),
      FilePath(),
      NULL, 0, FILE_PATH_LITERAL(""),
      web_ui()->GetWebContents()->GetView()->GetTopLevelNativeWindow(), NULL);
}

void TracingMessageHandler::SaveTraceFileComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui()->CallJavascriptFunction("tracingController.onSaveTraceFileComplete");
}

void TracingMessageHandler::OnBeginTracing(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(args->GetSize(), (size_t) 2);

  bool system_tracing_requested = false;
  bool ok = args->GetBoolean(0, &system_tracing_requested);
  DCHECK(ok);

  std::string chrome_categories;
  ok = args->GetString(1, &chrome_categories);
  DCHECK(ok);

  trace_enabled_ = true;
  // TODO(jbates) This may fail, but that's OK for current use cases.
  //              Ex: Multiple about:gpu traces can not trace simultaneously.
  // TODO(nduca) send feedback to javascript about whether or not BeginTracing
  //             was successful.
  TraceController::GetInstance()->BeginTracing(this, chrome_categories);

  if (system_tracing_requested) {
#if defined(OS_CHROMEOS)
    DCHECK(!system_trace_in_progress_);
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
        StartSystemTracing();
    // TODO(sleffler) async, could wait for completion
    system_trace_in_progress_ = true;
#endif
  }
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
  if (system_trace_in_progress_) {
    // Disable system tracing now that the local trace has shutdown.
    // This must be done last because we potentially need to push event
    // records into the system event log for synchronizing system event
    // timestamps with chrome event timestamps--and since the system event
    // log is a ring-buffer (on linux) adding them at the end is the only
    // way we're confident we'll have them in the final result.
    system_trace_in_progress_ = false;
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
        RequestStopSystemTracing(
            base::Bind(&TracingMessageHandler::OnEndSystemTracingAck,
            base::Unretained(this)));
    return;
#endif
  }
  web_ui()->CallJavascriptFunction("tracingController.onEndTracingComplete");
}

void TracingMessageHandler::OnEndSystemTracingAck(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui()->CallJavascriptFunction(
      "tracingController.onSystemTraceDataCollected",
      *scoped_ptr<Value>(Value::CreateStringValue(events_str_ptr->data())));
  DCHECK(!system_trace_in_progress_);

  OnEndTracingComplete();
}

void TracingMessageHandler::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::debug::TraceResultBuffer::SimpleOutput output;
  base::debug::TraceResultBuffer trace_buffer;
  trace_buffer.SetOutputCallback(output.GetCallback());
  output.Append("tracingController.onTraceDataCollected(");
  trace_buffer.Start();
  trace_buffer.AddFragment(trace_fragment->data());
  trace_buffer.Finish();
  output.Append(");");

  web_ui()->GetWebContents()->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(output.json_output));
}

void TracingMessageHandler::OnTraceBufferPercentFullReply(float percent_full) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui()->CallJavascriptFunction(
      "tracingController.onRequestBufferPercentFullComplete",
      *scoped_ptr<Value>(Value::CreateDoubleValue(percent_full)));
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// TracingUI
//
////////////////////////////////////////////////////////////////////////////////

TracingUI::TracingUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new TracingMessageHandler());

  // Set up the chrome://tracing/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateTracingHTMLSource());
}
