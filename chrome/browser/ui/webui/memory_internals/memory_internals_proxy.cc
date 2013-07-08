// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals/memory_internals_proxy.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/webui/memory_internals/memory_internals_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

using content::BrowserThread;

namespace {

class ProcessDetails : public MemoryDetails {
 public:
  typedef base::Callback<void(const ProcessData&)> DataCallback;
  explicit ProcessDetails(const DataCallback& callback)
      : callback_(callback) {}
  // MemoryDetails:
  virtual void OnDetailsAvailable() OVERRIDE {
    const std::vector<ProcessData>& browser_processes = processes();
    // [0] means Chrome.
    callback_.Run(browser_processes[0]);
  }

 private:
  virtual ~ProcessDetails() {}

  DataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ProcessDetails);
};

}  // namespace

class RendererDetails : public content::NotificationObserver {
 public:
  typedef base::Callback<void(const base::ProcessId pid,
                              const size_t v8_allocated,
                              const size_t v8_used)> V8DataCallback;

  explicit RendererDetails(const V8DataCallback& callback)
      : callback_(callback) {
    registrar_.Add(this, chrome::NOTIFICATION_RENDERER_V8_HEAP_STATS_COMPUTED,
                   content::NotificationService::AllSources());
  }
  virtual ~RendererDetails() {}

  void Request() {
    for (std::set<content::WebContents*>::iterator iter = web_contents_.begin();
         iter != web_contents_.end(); ++iter)
      (*iter)->GetRenderViewHost()->Send(new ChromeViewMsg_GetV8HeapStats);
  }
  void AddWebContents(content::WebContents* content) {
    web_contents_.insert(content);
  }
  void Clear() {
    web_contents_.clear();
  }
  void RemoveWebContents() {
    // We don't have to detect which content is the caller of this method.
    if (!web_contents_.empty())
      web_contents_.erase(web_contents_.begin());
  }
  int IsClean() {
    return web_contents_.empty();
  }

 private:
  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    const base::ProcessId* pid =
        content::Source<const base::ProcessId>(source).ptr();
    const ChromeRenderMessageFilter::V8HeapStatsDetails* v8_heap =
        content::Details<const ChromeRenderMessageFilter::V8HeapStatsDetails>(
            details).ptr();
    callback_.Run(*pid,
                  v8_heap->v8_memory_allocated(),
                  v8_heap->v8_memory_used());
  }

  V8DataCallback callback_;
  content::NotificationRegistrar registrar_;
  std::set<content::WebContents*> web_contents_;  // This class does not own

  DISALLOW_COPY_AND_ASSIGN(RendererDetails);
};

MemoryInternalsProxy::MemoryInternalsProxy()
    : information_(new base::DictionaryValue()),
      renderer_details_(new RendererDetails(
          base::Bind(&MemoryInternalsProxy::OnV8MemoryUpdate, this))) {
}

void MemoryInternalsProxy::Attach(MemoryInternalsHandler* handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = handler;
}

void MemoryInternalsProxy::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = NULL;
}

void MemoryInternalsProxy::GetInfo(const base::ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<ProcessDetails> browsers(new ProcessDetails(
      base::Bind(&MemoryInternalsProxy::OnDetailsAvailable, this)));
  browsers->StartFetch(MemoryDetails::SKIP_USER_METRICS);
}

MemoryInternalsProxy::~MemoryInternalsProxy() {}

void MemoryInternalsProxy::UpdateUIOnUIThread(const string16& update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}

void MemoryInternalsProxy::OnV8MemoryUpdate(const base::ProcessId pid,
                                            const size_t v8_allocated,
                                            const size_t v8_used) {
  // Do not update while no renderers are registered.
  if (renderer_details_->IsClean())
    return;

  base::ListValue* processes;
  if (!information_->GetList("processes", &processes))
    return;

  const size_t size = processes->GetSize();
  for (size_t i = 0; i < size; ++i) {
    base::DictionaryValue* process;
    processes->GetDictionary(i, &process);
    int id;
    if (!process->GetInteger("pid", &id) || id != static_cast<int>(pid))
      continue;
    // Convert units from Bytes to KiB.
    process->SetInteger("v8_alloc", v8_allocated / 1024);
    process->SetInteger("v8_used", v8_used / 1024);
    break;
  }

  renderer_details_->RemoveWebContents();
  if (renderer_details_->IsClean())
    CallJavaScriptFunctionOnUIThread("g_main_view.onSetSnapshot", information_);
}

void MemoryInternalsProxy::RequestV8MemoryUpdate() {
  renderer_details_->Clear();
#if defined(OS_ANDROID)
  for (TabModelList::const_iterator iter = TabModelList::begin();
       iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i)
      renderer_details_->AddWebContents(model->GetWebContentsAt(i));
  }
#else
  for (TabContentsIterator iter; !iter.done(); iter.Next())
    renderer_details_->AddWebContents(*iter);
#endif

  // if no contents are shown, update UI.
  if (renderer_details_->IsClean())
    CallJavaScriptFunctionOnUIThread("g_main_view.onSetSnapshot", information_);

  renderer_details_->Request();
}

void MemoryInternalsProxy::OnDetailsAvailable(const ProcessData& browser) {
  information_->Clear();

  // System information, which is independent from processes.
  information_->SetInteger("uptime", base::SysInfo::Uptime());
  information_->SetString("os", base::SysInfo::OperatingSystemName());
  information_->SetString("os_version",
                          base::SysInfo::OperatingSystemVersion());

  base::ListValue* processes = new ListValue();
  base::ListValue* extensions = new ListValue();
  information_->Set("processes", processes);
  information_->Set("extensions", extensions);
  for (ProcessMemoryInformationList::const_iterator
           iter = browser.processes.begin();
       iter != browser.processes.end(); ++iter) {
    base::DictionaryValue* process = new DictionaryValue();
    if (iter->process_type == content::PROCESS_TYPE_RENDERER &&
        iter->renderer_type == ProcessMemoryInformation::RENDERER_EXTENSION)
      extensions->Append(process);
    else
      processes->Append(process);

    // Information from MemoryDetails.
    process->SetInteger("pid", iter->pid);
    process->SetString("type",
                    ProcessMemoryInformation::GetFullTypeNameInEnglish(
                        iter->process_type, iter->renderer_type));
    process->SetInteger("memory_private", iter->working_set.priv);
    base::ListValue* titles = new ListValue();
    process->Set("titles", titles);
    for (size_t i = 0; i < iter->titles.size(); ++i)
      titles->AppendString(iter->titles[i]);
  }

  RequestV8MemoryUpdate();
}

void MemoryInternalsProxy::CallJavaScriptFunctionOnUIThread(
    const std::string& function, base::Value* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<const base::Value*> args_vector;
  args_vector.push_back(args);
  string16 update = content::WebUI::GetJavascriptCall(function, args_vector);
  UpdateUIOnUIThread(update);
}
