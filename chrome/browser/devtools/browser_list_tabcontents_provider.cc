// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"

#include "base/path_service.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsHttpHandlerDelegate;
using content::RenderViewHost;

BrowserListTabContentsProvider::BrowserListTabContentsProvider(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type)
    : profile_(profile), host_desktop_type_(host_desktop_type) {
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
    if (top_sites->GetPageThumbnail(url, &data))
      return std::string(
          reinterpret_cast<const char*>(data->front()), data->size());
  }

  return std::string();
}

RenderViewHost* BrowserListTabContentsProvider::CreateNewTarget() {
  const BrowserList* browser_list =
      BrowserList::GetInstance(host_desktop_type_);

  if (browser_list->empty())
    chrome::NewEmptyWindow(profile_, host_desktop_type_);

  if (browser_list->empty())
    return NULL;

  content::WebContents* web_contents = chrome::AddSelectedTabWithURL(
      browser_list->get(0),
      GURL(chrome::kAboutBlankURL),
      content::PAGE_TRANSITION_LINK);
  return web_contents->GetRenderViewHost();
}

content::DevToolsHttpHandlerDelegate::TargetType
BrowserListTabContentsProvider::GetTargetType(content::RenderViewHost* rvh) {
  for (TabContentsIterator it; !it.done(); it.Next())
    if (rvh == it->GetRenderViewHost())
      return kTargetTypeTab;

  return kTargetTypeOther;
}
