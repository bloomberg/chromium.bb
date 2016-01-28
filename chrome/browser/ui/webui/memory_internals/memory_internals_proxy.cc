// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals/memory_internals_proxy.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/process_resource_usage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/webui/memory_internals/memory_internals_handler.h"
#include "chrome/common/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/service_registry.h"

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/background_printing_manager.h"
#endif

using content::BrowserThread;

class Profile;

namespace {

class ProcessDetails : public MemoryDetails {
 public:
  typedef base::Callback<void(const ProcessData&)> DataCallback;
  explicit ProcessDetails(const DataCallback& callback)
      : callback_(callback) {}

  // MemoryDetails:
  void OnDetailsAvailable() override { callback_.Run(*ChromeBrowser()); }

 private:
  ~ProcessDetails() override {}

  DataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ProcessDetails);
};

base::DictionaryValue* FindProcessFromPid(base::ListValue* processes,
                                          base::ProcessId pid) {
  const size_t n = processes->GetSize();
  for (size_t i = 0; i < n; ++i) {
    base::DictionaryValue* process;
    if (!processes->GetDictionary(i, &process))
      return NULL;
    int id;
    if (process->GetInteger("pid", &id) && id == static_cast<int>(pid))
      return process;
  }
  return NULL;
}

void GetAllWebContents(std::set<content::WebContents*>* web_contents) {
  // Add all the existing WebContentses.
#if BUILDFLAG(ANDROID_JAVA_UI)
  for (TabModelList::const_iterator iter = TabModelList::begin();
       iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      content::WebContents* tab_web_contents = model->GetWebContentsAt(i);
      if (tab_web_contents)
        web_contents->insert(tab_web_contents);
    }
  }
#else
  for (TabContentsIterator iter; !iter.done(); iter.Next())
    web_contents->insert(*iter);
#endif
  // Add all the prerender pages.
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profiles[i]);
    if (!prerender_manager)
      continue;
    const std::vector<content::WebContents*> contentses =
        prerender_manager->GetAllPrerenderingContents();
    web_contents->insert(contentses.begin(), contentses.end());
  }
#if defined(ENABLE_PRINT_PREVIEW)
  // Add all the pages being background printed.
  printing::BackgroundPrintingManager* printing_manager =
      g_browser_process->background_printing_manager();
  std::set<content::WebContents*> printing_contents =
      printing_manager->CurrentContentSet();
  web_contents->insert(printing_contents.begin(), printing_contents.end());
#endif
}

}  // namespace

class RendererDetails {
 public:
  typedef base::Callback<void(const base::ProcessId pid,
                              const size_t v8_allocated,
                              const size_t v8_used)> V8DataCallback;

  explicit RendererDetails(const V8DataCallback& callback)
      : callback_(callback), weak_factory_(this) {}
  ~RendererDetails() {}

  void Request() {
    for (std::set<content::WebContents*>::iterator iter = web_contents_.begin();
         iter != web_contents_.end(); ++iter) {
      auto rph = (*iter)->GetRenderViewHost()->GetProcess();
      auto resource_usage = resource_usage_reporters_[(*iter)];
      DCHECK(resource_usage.get());
      resource_usage->Refresh(base::Bind(&RendererDetails::OnRefreshDone,
                                         weak_factory_.GetWeakPtr(), *iter,
                                         base::GetProcId(rph->GetHandle())));
    }
  }

  void AddWebContents(content::WebContents* content) {
    web_contents_.insert(content);

    auto rph = content->GetRenderViewHost()->GetProcess();
    content::ServiceRegistry* service_registry = rph->GetServiceRegistry();
    ResourceUsageReporterPtr service;
    if (service_registry)
      service_registry->ConnectToRemoteService(mojo::GetProxy(&service));
    resource_usage_reporters_.insert(std::make_pair(
        content,
        make_linked_ptr(new ProcessResourceUsage(std::move(service)))));
    DCHECK_EQ(web_contents_.size(), resource_usage_reporters_.size());
  }

  void Clear() {
    web_contents_.clear();
    resource_usage_reporters_.clear();
    weak_factory_.InvalidateWeakPtrs();
  }

  void RemoveWebContents(base::ProcessId) {
    // This function should only be called once for each WebContents, so this
    // should be non-empty every time it's called.
    DCHECK(!web_contents_.empty());

    // We don't have to detect which content is the caller of this method.
    if (!web_contents_.empty())
      web_contents_.erase(web_contents_.begin());
  }

  int IsClean() {
    return web_contents_.empty();
  }

 private:
  void OnRefreshDone(content::WebContents* content, const base::ProcessId pid) {
    auto iter = resource_usage_reporters_.find(content);
    DCHECK(iter != resource_usage_reporters_.end());
    linked_ptr<ProcessResourceUsage> usage = iter->second;
    callback_.Run(pid, usage->GetV8MemoryAllocated(), usage->GetV8MemoryUsed());
  }

  V8DataCallback callback_;
  std::set<content::WebContents*> web_contents_;  // This class does not own
  std::map<content::WebContents*, linked_ptr<ProcessResourceUsage>>
      resource_usage_reporters_;

  base::WeakPtrFactory<RendererDetails> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererDetails);
};

MemoryInternalsProxy::MemoryInternalsProxy()
    : information_(new base::DictionaryValue()),
      renderer_details_(new RendererDetails(
          base::Bind(&MemoryInternalsProxy::OnRendererAvailable, this))) {}

void MemoryInternalsProxy::Attach(MemoryInternalsHandler* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  handler_ = handler;
}

void MemoryInternalsProxy::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  handler_ = NULL;
}

void MemoryInternalsProxy::StartFetch(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Clear previous information before fetching new information.
  information_->Clear();
  scoped_refptr<ProcessDetails> process(new ProcessDetails(
      base::Bind(&MemoryInternalsProxy::OnProcessAvailable, this)));
  process->StartFetch(MemoryDetails::FROM_CHROME_ONLY);
}

MemoryInternalsProxy::~MemoryInternalsProxy() {}

void MemoryInternalsProxy::RequestRendererDetails() {
  renderer_details_->Clear();

#if BUILDFLAG(ANDROID_JAVA_UI)
  for (TabModelList::const_iterator iter = TabModelList::begin();
       iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      content::WebContents* tab_web_contents = model->GetWebContentsAt(i);
      if (tab_web_contents)
        renderer_details_->AddWebContents(tab_web_contents);
    }
  }
#else
  for (TabContentsIterator iter; !iter.done(); iter.Next())
    renderer_details_->AddWebContents(*iter);
#endif

  renderer_details_->Request();
}

void MemoryInternalsProxy::OnProcessAvailable(const ProcessData& browser) {
  base::ListValue* process_info = new base::ListValue();
  base::ListValue* extension_info = new base::ListValue();
  information_->Set("processes", process_info);
  information_->Set("extensions", extension_info);
  for (PMIIterator iter = browser.processes.begin();
       iter != browser.processes.end(); ++iter) {
    base::DictionaryValue* process = new base::DictionaryValue();
    if (iter->renderer_type == ProcessMemoryInformation::RENDERER_EXTENSION)
      extension_info->Append(process);
    else
      process_info->Append(process);

    // From MemoryDetails.
    process->SetInteger("pid", iter->pid);
    process->SetString("type",
                       ProcessMemoryInformation::GetFullTypeNameInEnglish(
                           iter->process_type, iter->renderer_type));
    process->SetInteger("memory_private", iter->working_set.priv);

    base::ListValue* titles = new base::ListValue();
    process->Set("titles", titles);
    for (size_t i = 0; i < iter->titles.size(); ++i)
      titles->AppendString(iter->titles[i]);
  }

  std::set<content::WebContents*> web_contents;
  GetAllWebContents(&web_contents);
  ConvertTabsInformation(web_contents, process_info);

  RequestRendererDetails();
}

void MemoryInternalsProxy::OnRendererAvailable(const base::ProcessId pid,
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

  renderer_details_->RemoveWebContents(pid);
  if (renderer_details_->IsClean())
    FinishCollection();
}

void MemoryInternalsProxy::ConvertTabsInformation(
    const std::set<content::WebContents*>& web_contents,
    base::ListValue* processes) {
  for (std::set<content::WebContents*>::const_iterator
           iter = web_contents.begin(); iter != web_contents.end(); ++iter) {
    content::WebContents* web = *iter;
    content::RenderProcessHost* process_host = web->GetRenderProcessHost();
    if (!process_host)
        continue;

    // Find which process renders the web contents.
    const base::ProcessId pid = base::GetProcId(process_host->GetHandle());
    base::DictionaryValue* process = FindProcessFromPid(processes, pid);
    if (!process)
      continue;

    // Prepare storage to register navigation histories.
    base::ListValue* tabs;
    if (!process->GetList("history", &tabs)) {
      tabs = new base::ListValue();
      process->Set("history", tabs);
    }

    base::DictionaryValue* tab = new base::DictionaryValue();
    tabs->Append(tab);

    base::ListValue* histories = new base::ListValue();
    tab->Set("history", histories);

    const content::NavigationController& controller = web->GetController();
    const int entry_size = controller.GetEntryCount();
    for (int i = 0; i < entry_size; ++i) {
      content::NavigationEntry *entry = controller.GetEntryAtIndex(i);
      base::DictionaryValue* history = new base::DictionaryValue();
      histories->Append(history);
      history->SetString("url", entry->GetURL().spec());
      history->SetString("title", entry->GetTitle());
      history->SetInteger("time", (base::Time::Now() -
                                   entry->GetTimestamp()).InSeconds());
    }
    tab->SetInteger("index", controller.GetCurrentEntryIndex());
  }
}

void MemoryInternalsProxy::FinishCollection() {
  information_->SetInteger("uptime", base::SysInfo::Uptime().InMilliseconds());
  information_->SetString("os", base::SysInfo::OperatingSystemName());
  information_->SetString("os_version",
                          base::SysInfo::OperatingSystemVersion());

  CallJavaScriptFunctionOnUIThread("g_main_view.onSetSnapshot", *information_);
}

void MemoryInternalsProxy::CallJavaScriptFunctionOnUIThread(
    const std::string& function, const base::Value& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<const base::Value*> args_vector(1, &args);
  base::string16 update =
      content::WebUI::GetJavascriptCall(function, args_vector);
  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}
