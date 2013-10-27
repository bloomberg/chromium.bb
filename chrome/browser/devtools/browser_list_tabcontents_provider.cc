// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"

#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/socket/tcp_listen_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsAgentHost;
using content::DevToolsHttpHandlerDelegate;
using content::DevToolsTarget;
using content::RenderViewHost;
using content::WebContents;

namespace {

const char kTargetTypePage[] = "page";
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

class Target : public content::DevToolsTarget {
 public:
  Target(WebContents* web_contents, bool is_tab);

  virtual std::string GetId() const OVERRIDE { return id_; }
  virtual std::string GetType() const OVERRIDE { return type_; }
  virtual std::string GetTitle() const OVERRIDE { return title_; }
  virtual std::string GetDescription() const OVERRIDE { return description_; }
  virtual GURL GetUrl() const OVERRIDE { return url_; }
  virtual GURL GetFaviconUrl() const OVERRIDE { return favicon_url_; }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }
  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string id_;
  std::string type_;
  std::string title_;
  std::string description_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

Target::Target(WebContents* web_contents, bool is_tab) {
  agent_host_ =
      DevToolsAgentHost::GetOrCreateFor(web_contents->GetRenderViewHost());
  id_ = agent_host_->GetId();
  type_ = is_tab ? kTargetTypePage : kTargetTypeOther;
  description_ = GetExtensionName(web_contents);
  title_ = UTF16ToUTF8(web_contents->GetTitle());
  url_ = web_contents->GetURL();
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    favicon_url_ = entry->GetFavicon().url;
  last_activity_time_ = web_contents->GetLastSelectedTime();
}

bool Target::Activate() const {
  RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  if (!rvh)
    return false;
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return false;
  web_contents->GetDelegate()->ActivateContents(web_contents);
  return true;
}

bool Target::Close() const {
  RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  if (!rvh)
    return false;
  rvh->ClosePage();
  return true;
}

class WorkerTarget : public content::DevToolsTarget {
 public:
  explicit WorkerTarget(const content::WorkerService::WorkerInfo& worker_info);

  virtual std::string GetId() const OVERRIDE { return id_; }
  virtual std::string GetType() const OVERRIDE { return "other"; }
  virtual std::string GetTitle() const OVERRIDE { return title_; }
  virtual std::string GetDescription() const OVERRIDE { return description_; }
  virtual GURL GetUrl() const OVERRIDE { return url_; }
  virtual GURL GetFaviconUrl() const OVERRIDE { return GURL(); }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return base::TimeTicks();
  }
  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE { return false; }
  virtual bool Close() const OVERRIDE { return false; }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string id_;
  std::string title_;
  std::string description_;
  GURL url_;
};

WorkerTarget::WorkerTarget(const content::WorkerService::WorkerInfo& worker) {
  agent_host_ =
      DevToolsAgentHost::GetForWorker(worker.process_id, worker.route_id);
  id_ = agent_host_->GetId();
  title_ = UTF16ToUTF8(worker.name);
  description_ =
      base::StringPrintf("Worker pid:%d", base::GetProcId(worker.handle));
  url_ = worker.url;
}

}  // namespace

BrowserListTabContentsProvider::BrowserListTabContentsProvider(
    chrome::HostDesktopType host_desktop_type)
    : host_desktop_type_(host_desktop_type) {
}

BrowserListTabContentsProvider::~BrowserListTabContentsProvider() {
}

std::string BrowserListTabContentsProvider::GetDiscoveryPageHTML() {
  std::set<Profile*> profiles;
  for (chrome::BrowserIterator it; !it.done(); it.Next())
    profiles.insert((*it)->profile());

  for (std::set<Profile*>::iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    history::TopSites* ts = (*it)->GetTopSites();
    if (ts) {
      // TopSites updates itself after a delay. Ask TopSites to update itself
      // when we're about to show the remote debugging landing page.
      ts->SyncWithHistory();
    }
  }
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML).as_string();
}

bool BrowserListTabContentsProvider::BundlesFrontendResources() {
  return true;
}

base::FilePath BrowserListTabContentsProvider::GetDebugFrontendDir() {
#if defined(DEBUG_DEVTOOLS)
  base::FilePath inspector_dir;
  PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir);
  return inspector_dir;
#else
  return base::FilePath();
#endif
}

std::string BrowserListTabContentsProvider::GetPageThumbnailData(
    const GURL& url) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Profile* profile = (*it)->profile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (!top_sites)
      continue;
    scoped_refptr<base::RefCountedMemory> data;
    if (top_sites->GetPageThumbnail(url, false, &data))
      return std::string(
          reinterpret_cast<const char*>(data->front()), data->size());
  }

  return std::string();
}

scoped_ptr<DevToolsTarget>
BrowserListTabContentsProvider::CreateNewTarget(const GURL& url) {
  const BrowserList* browser_list =
      BrowserList::GetInstance(host_desktop_type_);
  WebContents* web_contents;
  if (browser_list->empty()) {
    chrome::NewEmptyWindow(ProfileManager::GetLastUsedProfile(),
        host_desktop_type_);
    if (browser_list->empty())
      return scoped_ptr<DevToolsTarget>();
    web_contents =
        browser_list->get(0)->tab_strip_model()->GetActiveWebContents();
    web_contents->GetController().LoadURL(url,
        content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  } else {
    web_contents = chrome::AddSelectedTabWithURL(
      browser_list->get(0),
      url,
      content::PAGE_TRANSITION_LINK);
  }
  return scoped_ptr<DevToolsTarget>(new Target(web_contents, true));
}
void BrowserListTabContentsProvider::EnumerateTargets(TargetCallback callback) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BrowserListTabContentsProvider::GetWorkerInfo,
                 base::Unretained(this)),
      base::Bind(&BrowserListTabContentsProvider::RespondWithTargetList,
                 base::Unretained(this),
                 callback));
}

#if defined(DEBUG_DEVTOOLS)
static int g_last_tethering_port_ = 9333;

scoped_ptr<net::StreamListenSocket>
BrowserListTabContentsProvider::CreateSocketForTethering(
    net::StreamListenSocket::Delegate* delegate,
    std::string* name) {
  if (g_last_tethering_port_ == 9444)
    g_last_tethering_port_ = 9333;
  int port = ++g_last_tethering_port_;
  *name = base::IntToString(port);
  return net::TCPListenSocket::CreateAndListen("127.0.0.1", port, delegate)
      .PassAs<net::StreamListenSocket>();
}
#else
scoped_ptr<net::StreamListenSocket>
BrowserListTabContentsProvider::CreateSocketForTethering(
    net::StreamListenSocket::Delegate* delegate,
    std::string* name) {
  return scoped_ptr<net::StreamListenSocket>();
}
#endif  // defined(DEBUG_DEVTOOLS)

BrowserListTabContentsProvider::WorkerInfoList
BrowserListTabContentsProvider::GetWorkerInfo() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return content::WorkerService::GetInstance()->GetWorkers();
}

void BrowserListTabContentsProvider::RespondWithTargetList(
    TargetCallback callback, const WorkerInfoList& worker_info_list) {
  std::set<RenderViewHost*> tab_rvhs;
  for (TabContentsIterator it; !it.done(); it.Next())
    tab_rvhs.insert(it->GetRenderViewHost());

  TargetList targets;

  std::vector<RenderViewHost*> rvh_list =
      content::DevToolsAgentHost::GetValidRenderViewHosts();
  for (std::vector<RenderViewHost*>::iterator it = rvh_list.begin();
       it != rvh_list.end(); ++it) {
    bool is_tab = tab_rvhs.find(*it) != tab_rvhs.end();
    WebContents* web_contents = WebContents::FromRenderViewHost(*it);
    if (web_contents)
      targets.push_back(new Target(web_contents, is_tab));
  }

  for (size_t i = 0; i < worker_info_list.size(); ++i)
    targets.push_back(new WorkerTarget(worker_info_list[i]));

  callback.Run(targets);
}
