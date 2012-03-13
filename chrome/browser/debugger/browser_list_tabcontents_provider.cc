// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/browser_list_tabcontents_provider.h"

#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "grit/devtools_discovery_page_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsHttpHandlerDelegate;

BrowserListTabContentsProvider::BrowserListTabContentsProvider() {
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllSources());

}

BrowserListTabContentsProvider::~BrowserListTabContentsProvider() {
}

DevToolsHttpHandlerDelegate::InspectableTabs
BrowserListTabContentsProvider::GetInspectableTabs() {
  DevToolsHttpHandlerDelegate::InspectableTabs tabs;
  // Add the tabs from all browsers first.
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    TabStripModel* model = (*it)->tabstrip_model();
    for (int i = 0, size = model->count(); i < size; ++i)
      tabs.push_back(model->GetTabContentsAt(i)->web_contents());
  }

  // Then add any extra WebContents that have been observed.
  for (std::set<content::WebContents*>::iterator it = contents_.begin();
       it != contents_.end(); ++it) {
    if (std::find(tabs.begin(), tabs.end(), *it) == tabs.end())
      tabs.push_back(*it);
  }
  return tabs;
}

std::string BrowserListTabContentsProvider::GetDiscoveryPageHTML() {
  std::set<Profile*> profiles;
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    profiles.insert((*it)->GetProfile());
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
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML).as_string();
}

net::URLRequestContext*
BrowserListTabContentsProvider::GetURLRequestContext() {
  net::URLRequestContextGetter* getter =
      Profile::Deprecated::GetDefaultRequestContext();
  return getter ? getter->GetURLRequestContext() : NULL;
}

bool BrowserListTabContentsProvider::BundlesFrontendResources() {
  // We'd like front-end to be served from the WebUI via proxy, hence
  // pretend we don't have it bundled.
  return false;
}

std::string BrowserListTabContentsProvider::GetFrontendResourcesBaseURL() {
  return "chrome-devtools://devtools/";
}

void BrowserListTabContentsProvider::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::WebContents* web_contents =
      content::Source<content::WebContents>(source).ptr();
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      contents_.insert(web_contents);
      break;
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED:
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      contents_.erase(web_contents);
      break;
    default:
      NOTREACHED();
      return;
  }
}
