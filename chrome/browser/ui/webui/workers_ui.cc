// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/workers_ui.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/worker_devtools_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/devtools_messages.h"
#include "grit/generated_resources.h"
#include "grit/workers_resources.h"
#include "ui/base/resource/resource_bundle.h"

static const char kWorkersDataFile[] = "workers_data.json";

static const char kOpenDevToolsCommand[]  = "openDevTools";

static const char kWorkerProcessHostIdField[]  = "workerProcessHostId";
static const char kWorkerRouteIdField[]  = "workerRouteId";
static const char kUrlField[]  = "url";
static const char kNameField[]  = "name";
static const char kPidField[]  = "pid";

namespace {

class WorkersUIHTMLSource : public ChromeWebUIDataSource {
 public:
  WorkersUIHTMLSource();

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
 private:
  ~WorkersUIHTMLSource() {}
  void SendSharedWorkersData(int request_id);
  DISALLOW_COPY_AND_ASSIGN(WorkersUIHTMLSource);
};

WorkersUIHTMLSource::WorkersUIHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUIWorkersHost, NULL) {
  add_resource_path("workers.js", IDR_WORKERS_INDEX_JS);
  set_default_resource(IDR_WORKERS_INDEX_HTML);
}

void WorkersUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_incognito,
                                           int request_id) {
  if (path == kWorkersDataFile) {
    SendSharedWorkersData(request_id);
  } else {
    ChromeWebUIDataSource::StartDataRequest(path, is_incognito, request_id);
  }
}

void WorkersUIHTMLSource::SendSharedWorkersData(int request_id) {
    ListValue workers_list;
    BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
    for (; !iter.Done(); ++iter) {
      WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
      const WorkerProcessHost::Instances& instances = worker->instances();
      for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
           i != instances.end(); ++i) {
        if (!i->shared())
          continue;
        DictionaryValue* worker_data = new DictionaryValue();
        worker_data->SetInteger(kWorkerProcessHostIdField, worker->id());
        worker_data->SetInteger(kWorkerRouteIdField, i->worker_route_id());
        worker_data->SetString(kUrlField, i->url().spec());
        worker_data->SetString(kNameField, i->name());
        worker_data->SetInteger(kPidField, worker->pid());
        workers_list.Append(worker_data);
      }
    }

    std::string json_string;
    base::JSONWriter::Write(&workers_list, false, &json_string);

    SendResponse(request_id, base::RefCountedString::TakeString(&json_string));
}

class WorkersDOMHandler : public WebUIMessageHandler {
 public:
  WorkersDOMHandler() {}
  virtual ~WorkersDOMHandler() {}

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for "openDevTools" message.
  void HandleOpenDevTools(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(WorkersDOMHandler);
};

void WorkersDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kOpenDevToolsCommand,
      NewCallback(this, &WorkersDOMHandler::HandleOpenDevTools));
}

void WorkersDOMHandler::HandleOpenDevTools(const ListValue* args) {
  std::string worker_process_host_id_str;
  std::string worker_route_id_str;
  int worker_process_host_id;
  int worker_route_id;
  CHECK(args->GetSize() == 2);
  CHECK(args->GetString(0, &worker_process_host_id_str));
  CHECK(args->GetString(1, &worker_route_id_str));
  CHECK(base::StringToInt(worker_process_host_id_str,
                          &worker_process_host_id));
  CHECK(base::StringToInt(worker_route_id_str, &worker_route_id));

  Profile* profile = Profile::FromWebUI(web_ui_);
  if (!profile)
    return;
  DevToolsAgentHost* agent_host =
      WorkerDevToolsManager::GetDevToolsAgentHostForWorker(
          worker_process_host_id,
          worker_route_id);
  if (DevToolsManager::GetInstance()->GetDevToolsClientHostFor(agent_host))
    return;
  DevToolsWindow* window = DevToolsWindow::CreateDevToolsWindowForWorker(
      profile);
  window->Show(DEVTOOLS_TOGGLE_ACTION_NONE);
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(agent_host,
                                                                window);
}

}  // namespace

WorkersUI::WorkersUI(TabContents* contents) : ChromeWebUI(contents) {
  WorkersDOMHandler* handler = new WorkersDOMHandler();
  AddMessageHandler(handler);
  handler->Attach(this);

  WorkersUIHTMLSource* html_source = new WorkersUIHTMLSource();

  // Set up the chrome://workers/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}
