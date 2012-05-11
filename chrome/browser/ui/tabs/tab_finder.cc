// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_finder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"

using content::NavigationEntry;
using content::WebContents;

class TabFinder::WebContentsObserverImpl : public content::WebContentsObserver {
 public:
  WebContentsObserverImpl(WebContents* tab, TabFinder* finder);
  virtual ~WebContentsObserverImpl();

  WebContents* web_contents() {
    return content::WebContentsObserver::web_contents();
  }

  // content::WebContentsObserver overrides:
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* tab) OVERRIDE;

 private:
  TabFinder* finder_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverImpl);
};

TabFinder::WebContentsObserverImpl::WebContentsObserverImpl(
    WebContents* tab,
    TabFinder* finder)
    : content::WebContentsObserver(tab),
      finder_(finder) {
}

TabFinder::WebContentsObserverImpl::~WebContentsObserverImpl() {
}

void TabFinder::WebContentsObserverImpl::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  finder_->DidNavigateAnyFrame(web_contents(), details, params);
}

void TabFinder::WebContentsObserverImpl::WebContentsDestroyed(
    WebContents* tab) {
  finder_->TabDestroyed(this);
  delete this;
}

// static
TabFinder* TabFinder::GetInstance() {
  return IsEnabled() ? Singleton<TabFinder>::get() : NULL;
}

// static
bool TabFinder::IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFocusExistingTabOnOpen);
}

WebContents* TabFinder::FindTab(Browser* browser,
                                const GURL& url,
                                Browser** existing_browser) {
  if (browser->profile()->IsOffTheRecord())
    return NULL;

  // If the current tab matches the url, ignore it and let the user reload the
  // existing tab.
  WebContents* selected_tab = browser->GetSelectedWebContents();
  if (TabMatchesURL(selected_tab, url))
    return NULL;

  // See if the current browser has a tab matching the specified url.
  WebContents* tab_in_browser = FindTabInBrowser(browser, url);
  if (tab_in_browser) {
    *existing_browser = browser;
    return tab_in_browser;
  }

  // Then check other browsers.
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (!(*i)->profile()->IsOffTheRecord() &&
         (*i)->profile()->IsSameProfile(browser->profile())) {
      tab_in_browser = FindTabInBrowser(*i, url);
      if (tab_in_browser) {
        *existing_browser = *i;
        return tab_in_browser;
      }
    }
  }

  return NULL;
}

void TabFinder::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TAB_PARENTED);

  // The tab was added to a browser. Query for its state now.
  TabContentsWrapper* tab = content::Source<TabContentsWrapper>(source).ptr();
  TrackTab(tab->web_contents());
}

TabFinder::TabFinder() {
  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
}

TabFinder::~TabFinder() {
  STLDeleteElements(&tab_contents_observers_);
}

void TabFinder::DidNavigateAnyFrame(
    WebContents* source,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  CancelRequestsFor(source);

  if (content::PageTransitionIsRedirect(params.transition)) {
    // If this is a redirect, we need to go to the db to get the start.
    FetchRedirectStart(source);
  } else if (params.redirects.size() > 1 ||
             params.redirects[0] != details.entry->GetURL()) {
    web_contents_to_url_[source] = params.redirects[0];
  }
}

bool TabFinder::TabMatchesURL(WebContents* tab_contents, const GURL& url) {
  if (tab_contents->GetURL() == url)
    return true;

  WebContentsToURLMap::const_iterator i =
      web_contents_to_url_.find(tab_contents);
  return i != web_contents_to_url_.end() && i->second == url;
}

WebContents* TabFinder::FindTabInBrowser(Browser* browser, const GURL& url) {
  if (!browser->is_type_tabbed())
    return NULL;

  for (int i = 0; i < browser->tab_count(); ++i) {
    if (TabMatchesURL(browser->GetWebContentsAt(i), url))
      return browser->GetWebContentsAt(i);
  }
  return NULL;
}

void TabFinder::TrackTab(WebContents* tab) {
  for (WebContentsObservers::const_iterator i = tab_contents_observers_.begin();
       i != tab_contents_observers_.end(); ++i) {
    if ((*i)->web_contents() == tab) {
      // Already tracking the tab.
      return;
    }
  }
  WebContentsObserverImpl* observer = new WebContentsObserverImpl(tab, this);
  tab_contents_observers_.insert(observer);
  FetchRedirectStart(tab);
}

void TabFinder::TabDestroyed(WebContentsObserverImpl* observer) {
  DCHECK_GT(tab_contents_observers_.count(observer), 0u);
  tab_contents_observers_.erase(observer);
}

void TabFinder::CancelRequestsFor(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  web_contents_to_url_.erase(web_contents);

  HistoryService* history = profile->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (history) {
    CancelableRequestProvider::Handle request_handle;
    if (callback_consumer_.GetFirstHandleForClientData(web_contents,
                                                       &request_handle)) {
      history->CancelRequest(request_handle);
    }
  }
}

void TabFinder::FetchRedirectStart(WebContents* tab) {
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  NavigationEntry* committed_entry =
      tab->GetController().GetLastCommittedEntry();
  if (!committed_entry || committed_entry->GetURL().is_empty())
    return;

  HistoryService* history = profile->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (history) {
    CancelableRequestProvider::Handle request_handle =
        history->QueryRedirectsTo(
            committed_entry->GetURL(),
            &callback_consumer_,
            base::Bind(&TabFinder::QueryRedirectsToComplete,
                       base::Unretained(this)));
    callback_consumer_.SetClientData(history, request_handle, tab);
  }
}

void TabFinder::QueryRedirectsToComplete(HistoryService::Handle handle,
                                         GURL url,
                                         bool success,
                                         history::RedirectList* redirects) {
  if (success && !redirects->empty()) {
    WebContents* web_contents =
        callback_consumer_.GetClientDataForCurrentRequest();
    DCHECK(web_contents);
    web_contents_to_url_[web_contents] = redirects->back();
  }
}
