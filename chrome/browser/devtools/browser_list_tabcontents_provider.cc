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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsHttpHandlerDelegate;
using content::RenderViewHost;

BrowserListTabContentsProvider::BrowserListTabContentsProvider(
    Profile* profile)
    : profile_(profile) {
}

BrowserListTabContentsProvider::~BrowserListTabContentsProvider() {
}

std::string BrowserListTabContentsProvider::GetDiscoveryPageHTML() {
  std::set<Profile*> profiles;
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    profiles.insert((*it)->profile());
  }
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

FilePath BrowserListTabContentsProvider::GetDebugFrontendDir() {
#if defined(DEBUG_DEVTOOLS)
  FilePath inspector_dir;
  PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir);
  return inspector_dir;
#else
  return FilePath();
#endif
}

std::string BrowserListTabContentsProvider::GetPageThumbnailData(
    const GURL& url) {
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
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
  if (BrowserList::empty())
    chrome::NewEmptyWindow(profile_);

  if (BrowserList::empty())
    return NULL;

  content::WebContents* web_contents = chrome::AddSelectedTabWithURL(
      *BrowserList::begin(),
      GURL(chrome::kAboutBlankURL),
      content::PAGE_TRANSITION_LINK);
  return web_contents->GetRenderViewHost();
}
