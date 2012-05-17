// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/browser_list_tabcontents_provider.h"

#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsHttpHandlerDelegate;

BrowserListTabContentsProvider::BrowserListTabContentsProvider() {
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
      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(&history::TopSites::SyncWithHistory,
                     base::Unretained(ts)));
    }
  }
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML,
      ui::SCALE_FACTOR_NONE).as_string();
}

bool BrowserListTabContentsProvider::BundlesFrontendResources() {
  // We'd like front-end to be served from the WebUI via proxy, hence
  // pretend we don't have it bundled.
  return false;
}

std::string BrowserListTabContentsProvider::GetFrontendResourcesBaseURL() {
  return "chrome-devtools://devtools/";
}
