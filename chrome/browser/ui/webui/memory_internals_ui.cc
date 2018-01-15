// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals_ui.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/features.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/process_type.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"

using profiling::ProfilingProcessHost;

namespace {

// Returns the string to display at the top of the page for help.
std::string GetMessageString() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  switch (ProfilingProcessHost::GetInstance()->GetMode()) {
    case ProfilingProcessHost::Mode::kAll:
      return std::string("Memory logging is enabled for all processes.");

    case ProfilingProcessHost::Mode::kAllRenderers:
      return std::string("Memory logging is enabled for all renderers.");

    case ProfilingProcessHost::Mode::kBrowser:
      return std::string(
          "Memory logging is enabled for just the browser process.");

    case ProfilingProcessHost::Mode::kGpu:
      return std::string("Memory logging is enabled for just the gpu process.");

    case ProfilingProcessHost::Mode::kManual:
      return std::string(
          "Memory logging must be manually enabled for each process via "
          "chrome://memory-internals.");

    case ProfilingProcessHost::Mode::kMinimal:
      return std::string(
          "Memory logging is enabled for the browser and GPU processes.");

    case ProfilingProcessHost::Mode::kRendererSampling:
      return std::string(
          "Memory logging is enabled for an automatic sample of renderer "
          "processes. This UI is disabled.");

    case ProfilingProcessHost::Mode::kNone:
    default:
      return base::StringPrintf(
          "Memory logging is not enabled. Start with --%s=%s"
          " to log all processes, or --%s=%s to log only the browser and GPU "
          "processes. "
          "Other options available in chrome://flags",
          switches::kMemlog, switches::kMemlogModeAll, switches::kMemlog,
          switches::kMemlogModeMinimal);
  }
#elif defined(ADDRESS_SANITIZER) || defined(SYZYASAN)
  return "Memory logging is not available in this build because a memory "
         "sanitizer is running.";
#else
  return "Memory logging is not available in this build because "
         "USE_ALLOCATOR_SHIM is not set. It can not have sanitizers enabled "
         "and on Windows it must be a release non-component build.";
#endif
}

// Generates one row of the returned process info.
base::Value MakeProcessInfo(int pid, std::string description) {
  base::Value result(base::Value::Type::LIST);
  result.GetList().push_back(base::Value(pid));
  result.GetList().push_back(base::Value(std::move(description)));
  return result;
}

// Some child processes have good descriptions and some don't, this function
// returns the best it can given the data.
std::string GetChildDescription(const content::ChildProcessData& data) {
  if (!data.name.empty())
    return base::UTF16ToUTF8(data.name);
  return content::GetProcessTypeNameInEnglish(data.process_type);
}

content::WebUIDataSource* CreateMemoryInternalsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMemoryInternalsHost);
  source->SetDefaultResource(IDR_MEMORY_INTERNALS_HTML);
  source->AddResourcePath("memory_internals.js", IDR_MEMORY_INTERNALS_JS);
  source->UseGzip();
  return source;
}

class MemoryInternalsDOMHandler : public content::WebUIMessageHandler,
                                  ui::SelectFileDialog::Listener {
 public:
  explicit MemoryInternalsDOMHandler(content::WebUI* web_ui);
  ~MemoryInternalsDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestProcessList" message.
  void HandleRequestProcessList(const base::ListValue* args);

  // Callback for the "saveDump" message.
  void HandleSaveDump(const base::ListValue* args);

  // Callback for the "reportProcess" message.
  void HandleReportProcess(const base::ListValue* args);

  // Callback for the "startProfiling" message.
  void HandleStartProfiling(const base::ListValue* args);

 private:
  // Hops to the IO thread to enumerate child processes, and back to the UI
  // thread to fill in the renderer processes.
  static void GetChildProcessesOnIOThread(
      base::WeakPtr<MemoryInternalsDOMHandler> dom_handler);
  void GetProfiledPids(std::vector<base::Value> children);
  void ReturnProcessListOnUIThread(std::vector<base::Value> children,
                                   std::vector<base::ProcessId> profiled_pids);

  // SelectFileDialog::Listener implementation:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  void SaveTraceFinished(bool success);

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  content::WebUI* web_ui_;  // The WebUI that owns us.

  base::WeakPtrFactory<MemoryInternalsDOMHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemoryInternalsDOMHandler);
};

MemoryInternalsDOMHandler::MemoryInternalsDOMHandler(content::WebUI* web_ui)
    : web_ui_(web_ui), weak_factory_(this) {}

MemoryInternalsDOMHandler::~MemoryInternalsDOMHandler() {
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

void MemoryInternalsDOMHandler::RegisterMessages() {
  // Unretained should be OK here since this class is bound to the lifetime of
  // the WebUI.
  web_ui()->RegisterMessageCallback(
      "requestProcessList",
      base::Bind(&MemoryInternalsDOMHandler::HandleRequestProcessList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveDump",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleSaveDump,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reportProcess",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleReportProcess,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startProfiling",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleStartProfiling,
                          base::Unretained(this)));
}

void MemoryInternalsDOMHandler::HandleRequestProcessList(
    const base::ListValue* args) {
  // This is called on the UI thread, the child process iterator must run on
  // the IO thread, while the render process iterator must run on the UI thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&MemoryInternalsDOMHandler::GetChildProcessesOnIOThread,
                 weak_factory_.GetWeakPtr()));
}

void MemoryInternalsDOMHandler::HandleSaveDump(const base::ListValue* args) {
  base::FilePath default_file = base::FilePath().AppendASCII(
      base::StringPrintf("trace_with_heap_dump.json.gz"));

#if defined(OS_ANDROID)
  base::Value result("Saving...");
  AllowJavascript();
  CallJavascriptFunction("setSaveDumpMessage", result);

  // On Android write to the user data dir.
  // TODO(bug 757115) Does it make sense to show the Android file picker here
  // instead? Need to test what that looks like.
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath output_path = user_data_dir.Append(default_file);
  ProfilingProcessHost::GetInstance()->SaveTraceWithHeapDumpToFile(
      std::move(output_path),
      base::BindOnce(&MemoryInternalsDOMHandler::SaveTraceFinished,
                     weak_factory_.GetWeakPtr()),
      false);

  (void)web_ui_;  // Avoid warning about not using private web_ui_ member.
#else
  if (select_file_dialog_)
    return;  // Currently running, wait for existing save to complete.
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui_->GetWebContents()));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(), default_file,
      nullptr, 0, FILE_PATH_LITERAL(".json.gz"),
      web_ui_->GetWebContents()->GetTopLevelNativeWindow(), nullptr);
#endif
}

void MemoryInternalsDOMHandler::HandleReportProcess(
    const base::ListValue* args) {
  if (!args->is_list() || args->GetList().size() != 1)
    return;

  ProfilingProcessHost::GetInstance()->RequestProcessReport(
      "MEMLOG_MANUAL_TRIGGER");
}

void MemoryInternalsDOMHandler::HandleStartProfiling(
    const base::ListValue* args) {
  if (!args->is_list() || args->GetList().size() != 1)
    return;

  ProfilingProcessHost::GetInstance()->StartManualProfiling(
      args->GetList()[0].GetInt());
}

void MemoryInternalsDOMHandler::GetChildProcessesOnIOThread(
    base::WeakPtr<MemoryInternalsDOMHandler> dom_handler) {
  std::vector<base::Value> result;

  // The only non-renderer child process that currently supports out-of-process
  // heap profiling is GPU.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Note that ChildProcessData.id is a child ID and not an OS PID.
    const content::ChildProcessData& data = iter.GetData();

    if (data.process_type == content::PROCESS_TYPE_GPU) {
      result.push_back(MakeProcessInfo(base::GetProcId(data.handle),
                                       GetChildDescription(data)));
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&MemoryInternalsDOMHandler::GetProfiledPids, dom_handler,
                     std::move(result)));
}

void MemoryInternalsDOMHandler::GetProfiledPids(
    std::vector<base::Value> children) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ProfilingProcessHost::GetInstance()->GetProfiledPids(
      base::BindOnce(&MemoryInternalsDOMHandler::ReturnProcessListOnUIThread,
                     weak_factory_.GetWeakPtr(), std::move(children)));
}

void MemoryInternalsDOMHandler::ReturnProcessListOnUIThread(
    std::vector<base::Value> children,
    std::vector<base::ProcessId> profiled_pids) {
  // This function will be called with the child processes that are not
  // renderers. It will fill in the browser and renderer processes on the UI
  // thread (RenderProcessHost is UI-thread only) and return the full list.
  base::Value process_list_value(base::Value::Type::LIST);
  std::vector<base::Value>& process_list = process_list_value.GetList();

  // Add browser process.
  process_list.push_back(MakeProcessInfo(base::GetCurrentProcId(), "Browser"));

  // Append renderer processes.
  auto iter = content::RenderProcessHost::AllHostsIterator();
  while (!iter.IsAtEnd()) {
    base::ProcessHandle renderer_handle = iter.GetCurrentValue()->GetHandle();
    base::ProcessId renderer_pid = base::GetProcId(renderer_handle);
    if (renderer_pid != 0) {
      // TODO(brettw) make a better description of the process, maybe see
      // what TaskManager does to get the page title.
      process_list.push_back(MakeProcessInfo(renderer_pid, "Renderer"));
    }
    iter.Advance();
  }

  // Append all child processes collected on the IO thread.
  process_list.insert(process_list.end(),
                      std::make_move_iterator(std::begin(children)),
                      std::make_move_iterator(std::end(children)));

  // Sort profiled_pids to allow binary_search in the loop.
  std::sort(profiled_pids.begin(), profiled_pids.end());

  // Append whether each process is being profiled.
  for (base::Value& value : process_list) {
    std::vector<base::Value>& value_as_list = value.GetList();
    DCHECK_EQ(value_as_list.size(), 2u);

    base::ProcessId pid =
        static_cast<base::ProcessId>(value_as_list[0].GetInt());
    bool is_profiled =
        std::binary_search(profiled_pids.begin(), profiled_pids.end(), pid);
    value_as_list.push_back(base::Value(is_profiled));
  }

  // Pass the results in a dictionary.
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey("message", base::Value(GetMessageString()));
  result.SetKey("processes", std::move(process_list_value));

  AllowJavascript();
  CallJavascriptFunction("returnProcessList", result);
}

void MemoryInternalsDOMHandler::FileSelected(const base::FilePath& path,
                                             int index,
                                             void* params) {
  base::Value result("Saving...");
  AllowJavascript();
  CallJavascriptFunction("setSaveDumpMessage", result);

  ProfilingProcessHost::GetInstance()->SaveTraceWithHeapDumpToFile(
      path,
      base::BindOnce(&MemoryInternalsDOMHandler::SaveTraceFinished,
                     weak_factory_.GetWeakPtr()),
      false);
  select_file_dialog_ = nullptr;
}

void MemoryInternalsDOMHandler::FileSelectionCanceled(void* params) {
  select_file_dialog_ = nullptr;
}

void MemoryInternalsDOMHandler::SaveTraceFinished(bool success) {
  base::Value result(success ? "Save successful." : "Save failure.");
  AllowJavascript();
  CallJavascriptFunction("setSaveDumpMessage", result);
}

}  // namespace

MemoryInternalsUI::MemoryInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<MemoryInternalsDOMHandler>(web_ui));

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMemoryInternalsUIHTMLSource());
}

MemoryInternalsUI::~MemoryInternalsUI() {}
