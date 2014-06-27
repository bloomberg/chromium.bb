// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_target_impl.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/guest_view/guest_view_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"

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
const char kTargetTypeWebView[] = "webview";
const char kTargetTypeIFrame[] = "iframe";
const char kTargetTypeOther[] = "other";

// RenderViewHostTarget --------------------------------------------------------

class RenderViewHostTarget : public DevToolsTargetImpl {
 public:
  explicit RenderViewHostTarget(RenderViewHost* rvh, bool is_tab);

  // DevToolsTargetImpl overrides:
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;
  virtual RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual int GetTabId() const OVERRIDE;
  virtual std::string GetExtensionId() const OVERRIDE;
  virtual void Inspect(Profile* profile) const OVERRIDE;

 private:
  int tab_id_;
  std::string extension_id_;
};

RenderViewHostTarget::RenderViewHostTarget(RenderViewHost* rvh, bool is_tab)
    : DevToolsTargetImpl(DevToolsAgentHost::GetOrCreateFor(rvh)),
      tab_id_(-1) {
  set_type(kTargetTypeOther);
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return;  // Orphan RVH will show up with no title/url/icon in clients.

  content::RenderFrameHost* rfh = rvh->GetMainFrame();
  if (rfh->IsCrossProcessSubframe()) {
    set_url(rfh->GetLastCommittedURL());
    set_type(kTargetTypeIFrame);
    // TODO(kaznacheev) Try setting the title when the frame navigation
    // refactoring is done.
    RenderViewHost* parent_rvh = rfh->GetParent()->GetRenderViewHost();
    set_parent_id(DevToolsAgentHost::GetOrCreateFor(parent_rvh)->GetId());
    return;
  }

  set_title(base::UTF16ToUTF8(web_contents->GetTitle()));
  set_url(web_contents->GetURL());
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    set_favicon_url(entry->GetFavicon().url);
  set_last_activity_time(web_contents->GetLastActiveTime());

  GuestViewBase* guest = GuestViewBase::FromWebContents(web_contents);
  WebContents* guest_contents = guest ? guest->embedder_web_contents() : NULL;
  RenderViewHost* guest_parent_rvh =
      guest_contents ? guest_contents->GetRenderViewHost() : NULL;
  if (guest_parent_rvh) {
    set_type(kTargetTypeWebView);
    set_parent_id(DevToolsAgentHost::GetOrCreateFor(guest_parent_rvh)->GetId());
    return;
  }

  if (is_tab) {
    set_type(kTargetTypePage);
    tab_id_ = extensions::ExtensionTabUtil::GetTabId(web_contents);
    return;
  }

  const extensions::Extension* extension = extensions::ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          GetURL().host());
  if (!extension)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return;
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  set_title(extension->name());
  extensions::ExtensionHost* extension_host =
      extension_system->process_manager()->GetBackgroundHostForExtension(
          extension->id());
  if (extension_host &&
      extension_host->host_contents() == web_contents) {
    set_type(kTargetTypeBackgroundPage);
    extension_id_ = extension->id();
  } else if (extension->is_hosted_app()
             || extension->is_legacy_packaged_app()
             || extension->is_platform_app()) {
    set_type(kTargetTypeApp);
  }
  set_favicon_url(extensions::ExtensionIconSource::GetIconURL(
      extension, extension_misc::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_BIGGER, false, NULL));
}

bool RenderViewHostTarget::Activate() const {
  RenderViewHost* rvh = GetRenderViewHost();
  if (!rvh)
    return false;
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return false;
  web_contents->GetDelegate()->ActivateContents(web_contents);
  return true;
}

bool RenderViewHostTarget::Close() const {
  RenderViewHost* rvh = GetRenderViewHost();
  if (!rvh)
    return false;
  rvh->ClosePage();
  return true;
}

RenderViewHost* RenderViewHostTarget::GetRenderViewHost() const {
  return GetAgentHost()->GetRenderViewHost();
}

int RenderViewHostTarget::GetTabId() const {
  return tab_id_;
}

std::string RenderViewHostTarget::GetExtensionId() const {
  return extension_id_;
}

void RenderViewHostTarget::Inspect(Profile* profile) const {
  RenderViewHost* rvh = GetRenderViewHost();
  if (!rvh)
    return;
  DevToolsWindow::OpenDevToolsWindow(rvh);
}

// WorkerTarget ----------------------------------------------------------------

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

WorkerTarget::WorkerTarget(const WorkerService::WorkerInfo& worker)
    : DevToolsTargetImpl(DevToolsAgentHost::GetForWorker(worker.process_id,
                                                         worker.route_id)) {
  set_type(kTargetTypeWorker);
  set_title(base::UTF16ToUTF8(worker.name));
  set_description(base::StringPrintf("Worker pid:%d",
                      base::GetProcId(worker.handle)));
  set_url(worker.url);

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
  DevToolsWindow::OpenDevToolsWindowForWorker(profile, GetAgentHost());
}

}  // namespace

// DevToolsTargetImpl ----------------------------------------------------------

DevToolsTargetImpl::~DevToolsTargetImpl() {
}

DevToolsTargetImpl::DevToolsTargetImpl(
    scoped_refptr<DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
}

std::string DevToolsTargetImpl::GetParentId() const {
  return parent_id_;
}

std::string DevToolsTargetImpl::GetId() const {
  return agent_host_->GetId();
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

GURL DevToolsTargetImpl::GetURL() const {
  return url_;
}

GURL DevToolsTargetImpl::GetFaviconURL() const {
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

RenderViewHost* DevToolsTargetImpl::GetRenderViewHost() const {
  return NULL;
}

std::string DevToolsTargetImpl::GetExtensionId() const {
  return std::string();
}

void DevToolsTargetImpl::Inspect(Profile* /*profile*/) const {
}

void DevToolsTargetImpl::Reload() const {
}

// static
scoped_ptr<DevToolsTargetImpl> DevToolsTargetImpl::CreateForRenderViewHost(
    content::RenderViewHost* rvh, bool is_tab) {
  return scoped_ptr<DevToolsTargetImpl>(new RenderViewHostTarget(rvh, is_tab));
}

// static
DevToolsTargetImpl::List DevToolsTargetImpl::EnumerateRenderViewHostTargets() {
  std::set<RenderViewHost*> tab_rvhs;
  for (TabContentsIterator it; !it.done(); it.Next())
    tab_rvhs.insert(it->GetRenderViewHost());

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DevToolsTargetImpl::List result;
  std::vector<RenderViewHost*> rvh_list =
      content::DevToolsAgentHost::GetValidRenderViewHosts();
  for (std::vector<RenderViewHost*>::iterator it = rvh_list.begin();
       it != rvh_list.end(); ++it) {
    bool is_tab = tab_rvhs.find(*it) != tab_rvhs.end();
    result.push_back(new RenderViewHostTarget(*it, is_tab));
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
      DevToolsTargetImpl::EnumerateRenderViewHostTargets();
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
