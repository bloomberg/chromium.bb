// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_target_impl.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
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
#include "extensions/browser/guest_view/guest_view_base.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;
using content::WorkerService;

const char DevToolsTargetImpl::kTargetTypeApp[] = "app";
const char DevToolsTargetImpl::kTargetTypeBackgroundPage[] = "background_page";
const char DevToolsTargetImpl::kTargetTypePage[] = "page";
const char DevToolsTargetImpl::kTargetTypeWorker[] = "worker";
const char DevToolsTargetImpl::kTargetTypeWebView[] = "webview";
const char DevToolsTargetImpl::kTargetTypeIFrame[] = "iframe";
const char DevToolsTargetImpl::kTargetTypeOther[] = "other";
const char DevToolsTargetImpl::kTargetTypeServiceWorker[] = "service_worker";

namespace {

// WebContentsTarget --------------------------------------------------------

class WebContentsTarget : public DevToolsTargetImpl {
 public:
  WebContentsTarget(WebContents* web_contents, bool is_tab);

  // DevToolsTargetImpl overrides:
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual int GetTabId() const OVERRIDE;
  virtual std::string GetExtensionId() const OVERRIDE;
  virtual void Inspect(Profile* profile) const OVERRIDE;

 private:
  int tab_id_;
  std::string extension_id_;
};

WebContentsTarget::WebContentsTarget(WebContents* web_contents, bool is_tab)
    : DevToolsTargetImpl(DevToolsAgentHost::GetOrCreateFor(web_contents)),
      tab_id_(-1) {
  set_type(kTargetTypeOther);

  content::RenderFrameHost* rfh =
      web_contents->GetRenderViewHost()->GetMainFrame();
  if (rfh->IsCrossProcessSubframe()) {
    set_url(rfh->GetLastCommittedURL());
    set_type(kTargetTypeIFrame);
    // TODO(pfeldman) Update for out of process iframes.
    RenderViewHost* parent_rvh = rfh->GetParent()->GetRenderViewHost();
    set_parent_id(DevToolsAgentHost::GetOrCreateFor(
                      WebContents::FromRenderViewHost(parent_rvh))->GetId());
    return;
  }

  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    set_favicon_url(entry->GetFavicon().url);
  set_last_activity_time(web_contents->GetLastActiveTime());

  extensions::GuestViewBase* guest =
      extensions::GuestViewBase::FromWebContents(web_contents);
  WebContents* guest_contents = guest ? guest->embedder_web_contents() : NULL;
  if (guest_contents) {
    set_type(kTargetTypeWebView);
    set_parent_id(DevToolsAgentHost::GetOrCreateFor(guest_contents)->GetId());
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

WebContents* WebContentsTarget::GetWebContents() const {
  return GetAgentHost()->GetWebContents();
}

int WebContentsTarget::GetTabId() const {
  return tab_id_;
}

std::string WebContentsTarget::GetExtensionId() const {
  return extension_id_;
}

void WebContentsTarget::Inspect(Profile* profile) const {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  DevToolsWindow::OpenDevToolsWindow(web_contents);
}

// WorkerTarget ----------------------------------------------------------------

class WorkerTarget : public DevToolsTargetImpl {
 public:
  explicit WorkerTarget(scoped_refptr<DevToolsAgentHost> agent_host);

  // DevToolsTargetImpl overrides:
  virtual void Inspect(Profile* profile) const OVERRIDE;
};

WorkerTarget::WorkerTarget(scoped_refptr<DevToolsAgentHost> agent_host)
    : DevToolsTargetImpl(agent_host) {
  switch (agent_host->GetType()) {
    case DevToolsAgentHost::TYPE_SHARED_WORKER:
      set_type(kTargetTypeWorker);
      break;
    case DevToolsAgentHost::TYPE_SERVICE_WORKER:
      set_type(kTargetTypeServiceWorker);
      break;
    default:
      NOTREACHED();
  }
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
    : agent_host_(agent_host),
      title_(agent_host->GetTitle()),
      url_(agent_host->GetURL()) {
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
  return agent_host_->Activate();
}

bool DevToolsTargetImpl::Close() const {
  return agent_host_->Close();
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

void DevToolsTargetImpl::Inspect(Profile* /*profile*/) const {
}

void DevToolsTargetImpl::Reload() const {
}

// static
scoped_ptr<DevToolsTargetImpl> DevToolsTargetImpl::CreateForWebContents(
    content::WebContents* web_contents,
    bool is_tab) {
  return scoped_ptr<DevToolsTargetImpl>(
      new WebContentsTarget(web_contents, is_tab));
}

// static
void DevToolsTargetImpl::EnumerateAllTargets(Callback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::set<WebContents*> tab_web_contents;
  for (TabContentsIterator it; !it.done(); it.Next())
    tab_web_contents.insert(*it);

  DevToolsTargetImpl::List result;
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    DevToolsAgentHost* agent_host = (*it).get();
    switch (agent_host->GetType()) {
      case DevToolsAgentHost::TYPE_WEB_CONTENTS:
        if (WebContents* web_contents = agent_host->GetWebContents()) {
          const bool is_tab =
              tab_web_contents.find(web_contents) != tab_web_contents.end();
          result.push_back(new WebContentsTarget(web_contents, is_tab));
        }
        break;
      case DevToolsAgentHost::TYPE_SHARED_WORKER:
        result.push_back(new WorkerTarget(agent_host));
        break;
      case DevToolsAgentHost::TYPE_SERVICE_WORKER:
        result.push_back(new WorkerTarget(agent_host));
        break;
      default:
        break;
    }
  }

  callback.Run(result);
}
