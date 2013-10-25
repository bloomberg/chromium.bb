// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_target_impl.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;
using content::WorkerService;

namespace {

const char kTargetTypeApp[] = "app";
const char kTargetTypeBackgroundPage[] = "background_page";
const char kTargetTypePage[] = "page";
const char kTargetTypeWorker[] = "worker";
const char kTargetTypeOther[] = "other";

std::string GetExtensionName(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return std::string();

  extensions::ExtensionHost* extension_host =
      extensions::ExtensionSystem::Get(profile)->process_manager()->
          GetBackgroundHostForExtension(web_contents->GetURL().host());

  if (!extension_host || extension_host->host_contents() != web_contents)
    return std::string();

  return extension_host->extension()->name();
}

class WebContentsTarget : public DevToolsTargetImpl {
 public:
  explicit WebContentsTarget(WebContents* web_contents);

  // content::DevToolsTarget overrides:
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

  // DevToolsTargetImpl overrides:
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual int GetTabId() const OVERRIDE;
  virtual std::string GetExtensionId() const OVERRIDE;
  virtual void Inspect(Profile* profile) const OVERRIDE;

 private:
  int tab_id_;
  std::string extension_id_;
};

WebContentsTarget::WebContentsTarget(WebContents* web_contents) {
  agent_host_ =
      DevToolsAgentHost::GetOrCreateFor(web_contents->GetRenderViewHost());
  id_ = agent_host_->GetId();
  title_ = UTF16ToUTF8(web_contents->GetTitle());
  url_ = web_contents->GetURL();
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    favicon_url_ = entry->GetFavicon().url;
  last_activity_time_ = web_contents->GetLastSelectedTime();

  tab_id_ = ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id_ >= 0) {
    type_ = kTargetTypePage;
  } else {
    type_ = kTargetTypeOther;
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (profile) {
      ExtensionService* extension_service = profile->GetExtensionService();
      const extensions::Extension* extension = extension_service->
          extensions()->GetByID(url_.host());
      if (extension) {
        title_ = extension->name();
        if (extension->is_hosted_app()
            || extension->is_legacy_packaged_app()
            || extension->is_platform_app()) {
          type_ = kTargetTypeApp;
        } else {
          extensions::ExtensionHost* extension_host =
              extensions::ExtensionSystem::Get(profile)->process_manager()->
                  GetBackgroundHostForExtension(extension->id());
          if (extension_host &&
              extension_host->host_contents() == web_contents) {
            type_ = kTargetTypeBackgroundPage;
            extension_id_ = extension->id();
          }
        }
        favicon_url_ = extensions::ExtensionIconSource::GetIconURL(
            extension, extension_misc::EXTENSION_ICON_SMALLISH,
            ExtensionIconSet::MATCH_BIGGER, false, NULL);
      }
    }

    std::string extension_name = GetExtensionName(web_contents);
    if (!extension_name.empty()) {
      type_ = kTargetTypeBackgroundPage;
      title_ = extension_name;
    }
  }
}

bool WebContentsTarget::Activate() const {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;
  web_contents->GetDelegate()->ActivateContents(web_contents);
  return true;
}

bool WebContentsTarget::Close() const {
  RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  if (!rvh)
    return false;
  rvh->ClosePage();
  return true;
}

WebContents* WebContentsTarget::GetWebContents() const {
  RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  return rvh ? WebContents::FromRenderViewHost(rvh) : NULL;
}

int WebContentsTarget::GetTabId() const {
  return tab_id_;
}

std::string WebContentsTarget::GetExtensionId() const {
  return extension_id_;
}

void WebContentsTarget::Inspect(Profile* profile) const {
  RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  if (!rvh)
    return;
  DevToolsWindow::OpenDevToolsWindow(rvh);
}

///////////////////////////////////////////////////////////////////////////////

class WorkerTarget : public DevToolsTargetImpl {
 public:
  explicit WorkerTarget(const WorkerService::WorkerInfo& worker_info);

  // content::DevToolsTarget overrides:
  virtual bool Close() const OVERRIDE;

  // DevToolsTargetImpl overrides:
  virtual void Inspect(Profile* profile) const OVERRIDE;

 private:
  int process_id_;
  int route_id_;
};

WorkerTarget::WorkerTarget(const WorkerService::WorkerInfo& worker) {
  agent_host_ =
      DevToolsAgentHost::GetForWorker(worker.process_id, worker.route_id);
  id_ = agent_host_->GetId();
  type_ = kTargetTypeWorker;
  title_ = UTF16ToUTF8(worker.name);
  description_ =
      base::StringPrintf("Worker pid:%d", base::GetProcId(worker.handle));
  url_ = worker.url;

  process_id_ = worker.process_id;
  route_id_ = worker.route_id;
}

static void TerminateWorker(int process_id, int route_id) {
  WorkerService::GetInstance()->TerminateWorker(process_id, route_id);
}

bool WorkerTarget::Close() const {
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&TerminateWorker, process_id_, route_id_));
  return true;
}

void WorkerTarget::Inspect(Profile* profile) const {
  DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host_.get());
}

}  // namespace

DevToolsTargetImpl::~DevToolsTargetImpl() {
}

DevToolsTargetImpl::DevToolsTargetImpl() {
}

std::string DevToolsTargetImpl::GetId() const {
  return id_;
}

std::string DevToolsTargetImpl::GetType() const {
  return type_;
}

std::string DevToolsTargetImpl::GetTitle() const {
  return title_;
}

std::string DevToolsTargetImpl::GetDescription() const {
  return description_;
}

GURL DevToolsTargetImpl::GetUrl() const {
  return url_;
}

GURL DevToolsTargetImpl::GetFaviconUrl() const {
  return favicon_url_;
}

base::TimeTicks DevToolsTargetImpl::GetLastActivityTime() const {
  return last_activity_time_;
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsTargetImpl::GetAgentHost() const {
  return agent_host_;
}

bool DevToolsTargetImpl::IsAttached() const {
  return agent_host_->IsAttached();
}

bool DevToolsTargetImpl::Activate() const {
  return false;
}

bool DevToolsTargetImpl::Close() const {
  return false;
}

int DevToolsTargetImpl::GetTabId() const {
  return -1;
}

WebContents* DevToolsTargetImpl::GetWebContents() const {
  return NULL;
}

std::string DevToolsTargetImpl::GetExtensionId() const {
  return std::string();
}

void DevToolsTargetImpl::Inspect(Profile*) const {
}

void DevToolsTargetImpl::Reload() const {
}

// static
scoped_ptr<DevToolsTargetImpl> DevToolsTargetImpl::CreateForWebContents(
    content::WebContents* web_contents) {
  return scoped_ptr<DevToolsTargetImpl>(new WebContentsTarget(web_contents));
}

// static
scoped_ptr<DevToolsTargetImpl> DevToolsTargetImpl::CreateForWorker(
    const WorkerService::WorkerInfo& worker_info) {
  return scoped_ptr<DevToolsTargetImpl>(new WorkerTarget(worker_info));
}

// static
DevToolsTargetImpl::List DevToolsTargetImpl::EnumerateWebContentsTargets() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DevToolsTargetImpl::List result;
  std::vector<RenderViewHost*> rvh_list =
      content::DevToolsAgentHost::GetValidRenderViewHosts();
  for (std::vector<RenderViewHost*>::iterator it = rvh_list.begin();
       it != rvh_list.end(); ++it) {
    WebContents* web_contents = WebContents::FromRenderViewHost(*it);
    if (web_contents)
      result.push_back(new WebContentsTarget(web_contents));
  }
  return result;
}

static void CreateWorkerTargets(
    const std::vector<WorkerService::WorkerInfo>& worker_info,
    DevToolsTargetImpl::Callback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DevToolsTargetImpl::List result;
  for (size_t i = 0; i < worker_info.size(); ++i) {
    result.push_back(new WorkerTarget(worker_info[i]));
  }
  callback.Run(result);
}

// static
void DevToolsTargetImpl::EnumerateWorkerTargets(Callback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CreateWorkerTargets,
                 WorkerService::GetInstance()->GetWorkers(),
                 callback));
}

static void CollectAllTargets(
    DevToolsTargetImpl::Callback callback,
    const DevToolsTargetImpl::List& worker_targets) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DevToolsTargetImpl::List result =
      DevToolsTargetImpl::EnumerateWebContentsTargets();
  result.insert(result.begin(), worker_targets.begin(), worker_targets.end());
  callback.Run(result);
}

// static
void DevToolsTargetImpl::EnumerateAllTargets(Callback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DevToolsTargetImpl::EnumerateWorkerTargets,
                 base::Bind(&CollectAllTargets, callback)));
}
