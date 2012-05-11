// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_FINDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_FINDER_H_
#pragma once

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class GURL;

namespace content {
class WebContents;
struct FrameNavigateParams;
struct LoadCommittedDetails;
}

// TabFinder is used to locate a tab by URL. TabFinder matches tabs based
// on the tabs current url, or the start of the redirect chain.
//
// TODO: if we end up keeping this (moving it out of about:flags) then we
// should persist the start of the redirect chain in the navigation entry.
class TabFinder : public content::NotificationObserver {
 public:
  // Returns the TabFinder, or NULL if TabFinder is not enabled.
  static TabFinder* GetInstance();

  // Returns true if TabFinder is enabled.
  static bool IsEnabled();

  // Returns the tab that matches the specified url. If a tab is found the
  // browser containing the tab is set in |existing_browser|. This searches
  // in |browser| first before checking any other browsers.
  content::WebContents* FindTab(Browser* browser,
                                const GURL& url,
                                Browser** existing_browser);

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<TabFinder>;

  class WebContentsObserverImpl;

  typedef std::map<content::WebContents*, GURL> WebContentsToURLMap;
  typedef std::set<WebContentsObserverImpl*> WebContentsObservers;

  TabFinder();
  virtual ~TabFinder();

  // Forwarded from WebContentsObserverImpl.
  void DidNavigateAnyFrame(
      content::WebContents* source,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params);

  // Returns true if the tab's current url is |url|, or the start of the
  // redirect chain for the tab is |url|.
  bool TabMatchesURL(content::WebContents* web_contents, const GURL& url);

  // Returns the first tab in the specified browser that matches the specified
  // url.  Returns NULL if there are no tabs matching the specified url.
  content::WebContents* FindTabInBrowser(Browser* browser, const GURL& url);

  // If we're not currently tracking |tab| this creates a
  // WebContentsObserverImpl to listen for navigations.
  void TrackTab(content::WebContents* tab);

  // Invoked when a WebContents is being destroyed.
  void TabDestroyed(WebContentsObserverImpl* observer);

  // Cancels any pending requests for the specified tabs redirect chain.
  void CancelRequestsFor(content::WebContents* web_contents);

  // Starts the fetch for the redirect chain of the specified WebContents.
  // QueryRedirectsToComplete is invoked when the redirect chain is retrieved.
  void FetchRedirectStart(content::WebContents* tab);

  // Callback when we get the redirect list for a tab.
  void QueryRedirectsToComplete(CancelableRequestProvider::Handle handle,
                                GURL url,
                                bool success,
                                history::RedirectList* redirects);

  // Maps from WebContents to the start of the redirect chain.
  WebContentsToURLMap web_contents_to_url_;

  CancelableRequestConsumerTSimple<content::WebContents*> callback_consumer_;

  content::NotificationRegistrar registrar_;

  WebContentsObservers tab_contents_observers_;

  DISALLOW_COPY_AND_ASSIGN(TabFinder);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_FINDER_H_
