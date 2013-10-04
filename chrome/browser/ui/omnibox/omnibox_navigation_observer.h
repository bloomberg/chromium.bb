// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NAVIGATION_OBSERVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace content {
class NavigationController;
}

namespace net {
class URLFetcher;
class URLRequestStatus;
}

// Monitors omnibox navigations in order to trigger behaviors that depend on
// successful navigations.
//
// Currently two such behaviors exist:
// (1) For single-word queries where we can't tell if the entry was a search or
//     an intranet hostname, the omnibox opens as a search by default, but this
//     class attempts to open as a URL via an HTTP HEAD request.  If successful,
//     displays an infobar once the search result has also loaded.  See
//     AlternateNavInfoBarDelegate.
// (2) Omnibox navigations that complete successfully are added to the
//     Shortcuts backend.  TODO(pkasting): Not yet implemented.
//
// The memory management of this object is a bit tricky. The OmniboxEditModel
// will create us and be responsible for us until we attach as an observer
// after a pending load starts (it will delete us if this doesn't happen).
// Once this pending load starts, we're responsible for deleting ourselves.
class OmniboxNavigationObserver : public content::NotificationObserver,
                                  public net::URLFetcherDelegate {
 public:
  enum State {
    NOT_STARTED,
    IN_PROGRESS,
    SUCCEEDED,
    FAILED,
  };

  explicit OmniboxNavigationObserver(const GURL& alternate_nav_url);
  virtual ~OmniboxNavigationObserver();

  State state() const { return state_; }

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Sets |controller_| to the supplied pointer and begins fetching
  // |alternate_nav_url_|.
  void StartFetch(content::NavigationController* controller);

  // Sets |state_| to either SUCCEEDED or FAILED depending on the result of the
  // fetch.
  void SetStatusFromURLFetch(const GURL& url,
                             const net::URLRequestStatus& status,
                             int response_code);

  // Displays the infobar if all conditions are met (the page has loaded and
  // the fetch of the alternate URL succeeded).  Unless we're still waiting on
  // one of the above conditions to finish, this will also delete us, as whether
  // or not we show an infobar, there is no reason to live further.
  void ShowInfoBarIfPossible();

  GURL alternate_nav_url_;
  scoped_ptr<net::URLFetcher> fetcher_;
  content::NavigationController* controller_;
  State state_;
  bool navigated_to_entry_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxNavigationObserver);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NAVIGATION_OBSERVER_H_
