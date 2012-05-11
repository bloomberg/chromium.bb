// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
#define CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

namespace content {
class NavigationController;
}

namespace net {
class URLRequestStatus;
}

// Attempts to get the HEAD of a host name and displays an info bar if the
// request was successful. This is used for single-word queries where we can't
// tell if the entry was a search or an intranet hostname. The autocomplete bar
// assumes it's a query and issues an AlternateNavURLFetcher to display a "did
// you mean" infobar suggesting a navigation.
//
// The memory management of this object is a bit tricky. The location bar view
// will create us and be responsible for us until we attach as an observer
// after a pending load starts (it will delete us if this doesn't happen).
// Once this pending load starts, we're responsible for deleting ourselves.
// We'll do this in the following cases:
//   * The tab is navigated again once we start listening (so the fetch is no
//     longer useful)
//   * The tab is closed before we show an infobar
//   * The intranet fetch fails
//   * None of the above apply, so we successfully show an infobar
class AlternateNavURLFetcher : public content::NotificationObserver,
                               public content::URLFetcherDelegate {
 public:
  enum State {
    NOT_STARTED,
    IN_PROGRESS,
    SUCCEEDED,
    FAILED,
  };

  explicit AlternateNavURLFetcher(const GURL& alternate_nav_url);
  virtual ~AlternateNavURLFetcher();

  State state() const { return state_; }

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::URLFetcherDelegate
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
  void ShowInfobarIfPossible();

  GURL alternate_nav_url_;
  scoped_ptr<content::URLFetcher> fetcher_;
  content::NavigationController* controller_;
  State state_;
  bool navigated_to_entry_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavURLFetcher);
};

#endif  // CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
