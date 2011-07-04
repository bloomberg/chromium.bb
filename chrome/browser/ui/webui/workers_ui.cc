// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/workers_ui.h"

#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "grit/generated_resources.h"
#include "grit/workers_resources.h"
#include "ui/base/resource/resource_bundle.h"

static const char kWorkersDataFile[] = "workers_data.json";
static const char kIndexHtmlFile[]  = "index.html";

namespace {

class WorkersUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  WorkersUIHTMLSource();

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

 private:
  ~WorkersUIHTMLSource() {}

  void SendSharedWorkersData(int request_id);

  DISALLOW_COPY_AND_ASSIGN(WorkersUIHTMLSource);
};

WorkersUIHTMLSource::WorkersUIHTMLSource()
    : DataSource(chrome::kChromeUIWorkersHost, NULL) {
}

void WorkersUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_incognito,
                                           int request_id) {
  if (path == kWorkersDataFile) {
    SendSharedWorkersData(request_id);
  } else {
    int idr = IDR_WORKERS_INDEX_HTML;
    scoped_refptr<RefCountedStaticMemory> response(
        ResourceBundle::GetSharedInstance().LoadDataResourceBytes(idr));
    SendResponse(request_id, response);
  }
}

std::string WorkersUIHTMLSource::GetMimeType(const std::string& path) const {
  if (EndsWith(path, ".css", false))
    return "text/css";
  if (EndsWith(path, ".js", false))
    return "application/javascript";
  if (EndsWith(path, ".json", false))
    return "plain/text";
  return "text/html";
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
        worker_data->SetInteger("id", i->worker_route_id());
        worker_data->SetString("url", i->url().spec());
        worker_data->SetString("name", i->name());
        worker_data->SetInteger("pid", worker->pid());
        workers_list.Append(worker_data);
      }
    }

    std::string json_string;
    base::JSONWriter::Write(&workers_list, false, &json_string);

    scoped_refptr<RefCountedBytes> json_bytes(new RefCountedBytes());
    json_bytes->data.resize(json_string.size());
    std::copy(json_string.begin(), json_string.end(), json_bytes->data.begin());

    SendResponse(request_id, json_bytes);
}

}  // namespace

WorkersUI::WorkersUI(TabContents* contents) : ChromeWebUI(contents) {
  WorkersUIHTMLSource* html_source = new WorkersUIHTMLSource();

  // Set up the chrome://workers/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
