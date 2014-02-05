// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTRANET_REDIRECT_DETECTOR_H_
#define CHROME_BROWSER_INTRANET_REDIRECT_DETECTOR_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/host_resolver_proc.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class PrefRegistrySimple;

// This object is responsible for determining whether the user is on a network
// that redirects requests for intranet hostnames to another site, and if so,
// tracking what that site is (including across restarts via a pref).  For
// example, the user's ISP might convert a request for "http://query/" into a
// 302 redirect to "http://isp.domain.com/search?q=query" in order to display
// custom pages on typos, nonexistent sites, etc.
//
// We use this information in the OmniboxNavigationObserver to avoid displaying
// infobars for these cases.  Our infobars are designed to allow users to get at
// intranet sites when they were erroneously taken to a search result page.  In
// these cases, however, users would be shown a confusing and useless infobar
// when they really did mean to do a search.
//
// Consumers should call RedirectOrigin(), which is guaranteed to synchronously
// return a value at all times (even during startup or in unittest mode).  If no
// redirection is in place, the returned GURL will be empty.
class IntranetRedirectDetector
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Only the main browser process loop should call this, when setting up
  // g_browser_process->intranet_redirect_detector_.  No code other than the
  // IntranetRedirectDetector itself should actually use
  // g_browser_process->intranet_redirect_detector() (which shouldn't be hard,
  // since there aren't useful public functions on this object for consumers to
  // access anyway).
  IntranetRedirectDetector();
  virtual ~IntranetRedirectDetector();

  // Returns the current redirect origin.  This will be empty if no redirection
  // is in place.
  static GURL RedirectOrigin();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  typedef std::set<net::URLFetcher*> Fetchers;

  // Called when the seven second startup sleep or the one second network
  // switch sleep has finished.  Runs any pending fetch.
  void FinishSleep();

  // net::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver
  virtual void OnIPAddressChanged() OVERRIDE;

  GURL redirect_origin_;
  Fetchers fetchers_;
  std::vector<GURL> resulting_origins_;
  bool in_sleep_;  // True if we're in the seven-second "no fetching" period
                   // that begins at browser start, or the one-second "no
                   // fetching" period that begins after network switches.
  base::WeakPtrFactory<IntranetRedirectDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IntranetRedirectDetector);
};

#endif  // CHROME_BROWSER_INTRANET_REDIRECT_DETECTOR_H_
