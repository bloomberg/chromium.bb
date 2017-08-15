// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals_ui.h"

#include <iterator>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/process_type.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

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
  return source;
}

class MemoryInternalsDOMHandler : public content::WebUIMessageHandler {
 public:
  MemoryInternalsDOMHandler();
  ~MemoryInternalsDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestProcessList" message.
  void HandleRequestProcessList(const base::ListValue* args);

  // Callback for the "dumpProcess" message.
  void HandleDumpProcess(const base::ListValue* args);

 private:
  // Hops to the IO thread to enumerate child processes, and back to the UI
  // thread to fill in the renderer processes.
  static void GetChildProcessesOnIOThread(
      base::WeakPtr<MemoryInternalsDOMHandler> dom_handler);
  void ReturnProcessListOnUIThread(std::vector<base::Value> children);
  static void GetOutputFileOnFileThread(int32_t sender_id);
  static void HandleDumpProcessOnUIThread(int32_t sender_id, base::File file);

  base::WeakPtrFactory<MemoryInternalsDOMHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemoryInternalsDOMHandler);
};

MemoryInternalsDOMHandler::MemoryInternalsDOMHandler() : weak_factory_(this) {}

MemoryInternalsDOMHandler::~MemoryInternalsDOMHandler() {}

void MemoryInternalsDOMHandler::RegisterMessages() {
  // Unretained should be OK here since this class is bound to the lifetime of
  // the WebUI.
  web_ui()->RegisterMessageCallback(
      "requestProcessList",
      base::Bind(&MemoryInternalsDOMHandler::HandleRequestProcessList,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dumpProcess",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleDumpProcess,
                          base::Unretained(this)));
}

void MemoryInternalsDOMHandler::HandleRequestProcessList(
    const base::ListValue* args) {
  // This is called on the UI thread, the child process iterator must run on
  // the IO thread, while the render process iterator must run on the UI thread.
  //
  // TODO(brettw) Have some way to start the profiler if it's not started, or
  // at least tell people to restart with --memlog if it's not running.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&MemoryInternalsDOMHandler::GetChildProcessesOnIOThread,
                 weak_factory_.GetWeakPtr()));
}

void MemoryInternalsDOMHandler::HandleDumpProcess(const base::ListValue* args) {
  if (!args->is_list() || args->GetList().size() != 1)
    return;
  const base::Value& pid_value = args->GetList()[0];
  if (!pid_value.is_int())
    return;

  profiling::ProfilingProcessHost::GetInstance()->RequestProcessDump(
      pid_value.GetInt());
}

void MemoryInternalsDOMHandler::GetChildProcessesOnIOThread(
    base::WeakPtr<MemoryInternalsDOMHandler> dom_handler) {
  std::vector<base::Value> result;

  // Add child processes (this does not include renderers).
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Note that ChildProcessData.id is a child ID and not an OS PID.
    const content::ChildProcessData& data = iter.GetData();
    base::ProcessId proc_id = base::GetProcId(data.handle);
    std::string desc =
        base::StringPrintf("Process %lld [%s]", static_cast<long long>(proc_id),
                           GetChildDescription(data).c_str());
    result.push_back(MakeProcessInfo(proc_id, std::move(desc)));
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&MemoryInternalsDOMHandler::ReturnProcessListOnUIThread,
                     dom_handler, std::move(result)));
}

void MemoryInternalsDOMHandler::ReturnProcessListOnUIThread(
    std::vector<base::Value> children) {
  // This function will be called with the child processes that are not
  // renderers. It will full in the browser and renderer processes on the UI
  // thread (RenderProcessHost is UI-thread only).
  base::Value result(base::Value::Type::LIST);
  std::vector<base::Value>& result_list = result.GetList();

  // Add browser process.
  base::ProcessId browser_pid = base::GetCurrentProcId();
  result_list.push_back(MakeProcessInfo(
      browser_pid, base::StringPrintf("Process %lld [Browser]",
                                      static_cast<long long>(browser_pid))
                       .c_str()));

  // Append renderer processes.
  auto iter = content::RenderProcessHost::AllHostsIterator();
  while (!iter.IsAtEnd()) {
    base::ProcessHandle renderer_handle = iter.GetCurrentValue()->GetHandle();
    base::ProcessId renderer_pid = base::GetProcId(renderer_handle);
    if (renderer_pid != 0) {
      // TODO(brettw) make a better description of the process, maybe see
      // what TaskManager does.
      result_list.push_back(MakeProcessInfo(
          renderer_pid, base::StringPrintf("Process %lld [Renderer]",
                                           static_cast<long long>(renderer_pid))
                            .c_str()));
    }
    iter.Advance();
  }

  // Append all child processes collected on the IO thread.
  result_list.insert(result_list.end(),
                     std::make_move_iterator(std::begin(children)),
                     std::make_move_iterator(std::end(children)));

  AllowJavascript();
  CallJavascriptFunction("returnProcessList", result);
  DisallowJavascript();
}

}  // namespace

MemoryInternalsUI::MemoryInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<MemoryInternalsDOMHandler>());

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMemoryInternalsUIHTMLSource());
}

MemoryInternalsUI::~MemoryInternalsUI() {}
