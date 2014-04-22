// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"

#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/browser_resources.h"
#include "net/socket/tcp_listen_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsTarget;
using content::RenderViewHost;
using content::WebContents;

namespace {

const int kMinTetheringPort = 9333;
const int kMaxTetheringPort = 9444;

base::LazyInstance<bool>::Leaky g_tethering_enabled = LAZY_INSTANCE_INITIALIZER;

}

// static
void BrowserListTabContentsProvider::EnableTethering() {
  g_tethering_enabled.Get() = true;
}

BrowserListTabContentsProvider::BrowserListTabContentsProvider(
    chrome::HostDesktopType host_desktop_type)
    : host_desktop_type_(host_desktop_type),
      last_tethering_port_(kMinTetheringPort) {
  g_tethering_enabled.Get() = false;
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
      return std::string(data->front_as<char>(), data->size());
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
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (!rvh)
    return scoped_ptr<DevToolsTarget>();
  return scoped_ptr<DevToolsTarget>(
      DevToolsTargetImpl::CreateForRenderViewHost(rvh, true));
}

void BrowserListTabContentsProvider::EnumerateTargets(TargetCallback callback) {
  DevToolsTargetImpl::EnumerateAllTargets(
      *reinterpret_cast<DevToolsTargetImpl::Callback*>(&callback));
}

scoped_ptr<net::StreamListenSocket>
BrowserListTabContentsProvider::CreateSocketForTethering(
    net::StreamListenSocket::Delegate* delegate,
    std::string* name) {
  if (!g_tethering_enabled.Get())
    return scoped_ptr<net::StreamListenSocket>();

  if (last_tethering_port_ == kMaxTetheringPort)
    last_tethering_port_ = kMinTetheringPort;
  int port = ++last_tethering_port_;
  *name = base::IntToString(port);
  return net::TCPListenSocket::CreateAndListen("127.0.0.1", port, delegate)
      .PassAs<net::StreamListenSocket>();
}
