// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
#define CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class NavigationController;

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
class AlternateNavURLFetcher : public NotificationObserver,
                               public URLFetcher::Delegate {
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
  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data) OVERRIDE;

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
  scoped_ptr<URLFetcher> fetcher_;
  NavigationController* controller_;
  State state_;
  bool navigated_to_entry_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AlternateNavURLFetcher);
};

#endif  // CHROME_BROWSER_ALTERNATE_NAV_URL_FETCHER_H_
