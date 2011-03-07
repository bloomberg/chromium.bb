// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/tab_finder.h"

#include "base/command_line.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class TabFinder::TabContentsObserverImpl : public TabContentsObserver {
 public:
  TabContentsObserverImpl(TabContents* tab, TabFinder* finder);
  virtual ~TabContentsObserverImpl();

  TabContents* tab_contents() { return TabContentsObserver::tab_contents(); }

  // TabContentsObserver overrides:
  virtual void DidNavigateAnyFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void OnTabContentsDestroyed(TabContents* tab) OVERRIDE;

 private:
  TabFinder* finder_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsObserverImpl);
};

TabFinder::TabContentsObserverImpl::TabContentsObserverImpl(
    TabContents* tab,
    TabFinder* finder)
    : TabContentsObserver(tab),
      finder_(finder) {
}

TabFinder::TabContentsObserverImpl::~TabContentsObserverImpl() {
}

void TabFinder::TabContentsObserverImpl::DidNavigateAnyFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  finder_->DidNavigateAnyFramePostCommit(tab_contents(), details, params);
}

void TabFinder::TabContentsObserverImpl::OnTabContentsDestroyed(
    TabContents* tab) {
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

TabContents* TabFinder::FindTab(Browser* browser,
                                const GURL& url,
                                Browser** existing_browser) {
  if (browser->profile()->IsOffTheRecord())
    return NULL;

  // If the current tab matches the url, ignore it and let the user reload the
  // existing tab.
  TabContents* selected_tab = browser->GetSelectedTabContents();
  if (TabMatchesURL(selected_tab, url))
    return NULL;

  // See if the current browser has a tab matching the specified url.
  TabContents* tab_in_browser = FindTabInBrowser(browser, url);
  if (tab_in_browser) {
    *existing_browser = browser;
    return tab_in_browser;
  }

  // Then check other browsers.
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (!(*i)->profile()->IsOffTheRecord()) {
      tab_in_browser = FindTabInBrowser(*i, url);
      if (tab_in_browser) {
        *existing_browser = *i;
        return tab_in_browser;
      }
    }
  }

  return NULL;
}

void TabFinder::Observe(NotificationType type,
                        const NotificationSource& source,
                        const NotificationDetails& details) {
  DCHECK_EQ(type.value, NotificationType::TAB_PARENTED);

  // The tab was added to a browser. Query for its state now.
  NavigationController* controller =
      Source<NavigationController>(source).ptr();
  TrackTab(controller->tab_contents());
}

TabFinder::TabFinder() {
  registrar_.Add(this, NotificationType::TAB_PARENTED,
                 NotificationService::AllSources());
}

TabFinder::~TabFinder() {
  STLDeleteElements(&tab_contents_observers_);
}

void TabFinder::Init() {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (!(*i)->profile()->IsOffTheRecord())
      TrackBrowser(*i);
  }
}

void TabFinder::DidNavigateAnyFramePostCommit(
    TabContents* source,
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  CancelRequestsFor(source);

  if (PageTransition::IsRedirect(params.transition)) {
    // If this is a redirect, we need to go to the db to get the start.
    FetchRedirectStart(source);
  } else if (params.redirects.size() > 1 ||
             params.redirects[0] != details.entry->url()) {
    tab_contents_to_url_[source] = params.redirects[0];
  }
}

bool TabFinder::TabMatchesURL(TabContents* tab_contents, const GURL& url) {
  if (tab_contents->GetURL() == url)
    return true;

  TabContentsToURLMap::const_iterator i =
      tab_contents_to_url_.find(tab_contents);
  return i != tab_contents_to_url_.end() && i->second == url;
}

TabContents* TabFinder::FindTabInBrowser(Browser* browser, const GURL& url) {
  if (browser->type() != Browser::TYPE_NORMAL)
    return NULL;

  for (int i = 0; i < browser->tab_count(); ++i) {
    if (TabMatchesURL(browser->GetTabContentsAt(i), url))
      return browser->GetTabContentsAt(i);
  }
  return NULL;
}

void TabFinder::TrackTab(TabContents* tab) {
  for (TabContentsObservers::const_iterator i = tab_contents_observers_.begin();
       i != tab_contents_observers_.end(); ++i) {
    if ((*i)->tab_contents() == tab) {
      // Already tracking the tab.
      return;
    }
  }
  TabContentsObserverImpl* observer = new TabContentsObserverImpl(tab, this);
  tab_contents_observers_.insert(observer);
  FetchRedirectStart(tab);
}

void TabFinder::TrackBrowser(Browser* browser) {
  for (int i = 0; i < browser->tab_count(); ++i)
    FetchRedirectStart(browser->GetTabContentsAt(i));
}

void TabFinder::TabDestroyed(TabContentsObserverImpl* observer) {
  DCHECK_GT(tab_contents_observers_.count(observer), 0u);
  tab_contents_observers_.erase(observer);
}

void TabFinder::CancelRequestsFor(TabContents* tab_contents) {
  if (tab_contents->profile()->IsOffTheRecord())
    return;

  tab_contents_to_url_.erase(tab_contents);

  HistoryService* history = tab_contents->profile()->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (history) {
    CancelableRequestProvider::Handle request_handle;
    if (callback_consumer_.GetFirstHandleForClientData(tab_contents,
                                                       &request_handle)) {
      history->CancelRequest(request_handle);
    }
  }
}

void TabFinder::FetchRedirectStart(TabContents* tab) {
  if (tab->profile()->IsOffTheRecord())
    return;

  NavigationEntry* committed_entry = tab->controller().GetLastCommittedEntry();
  if (!committed_entry || committed_entry->url().is_empty())
    return;

  HistoryService* history =tab->profile()->GetHistoryService(
      Profile::EXPLICIT_ACCESS);
  if (history) {
    CancelableRequestProvider::Handle request_handle =
        history->QueryRedirectsTo(
            committed_entry->url(),
            &callback_consumer_,
            NewCallback(this, &TabFinder::QueryRedirectsToComplete));
    callback_consumer_.SetClientData(history, request_handle, tab);
  }
}

void TabFinder::QueryRedirectsToComplete(HistoryService::Handle handle,
                                         GURL url,
                                         bool success,
                                         history::RedirectList* redirects) {
  if (success && !redirects->empty()) {
    TabContents* tab_contents =
        callback_consumer_.GetClientDataForCurrentRequest();
    DCHECK(tab_contents);
    tab_contents_to_url_[tab_contents] = redirects->back();
  }
}
