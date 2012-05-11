// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTRANET_REDIRECT_DETECTOR_H_
#define CHROME_BROWSER_INTRANET_REDIRECT_DETECTOR_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/network_change_notifier.h"

class PrefService;

// This object is responsible for determining whether the user is on a network
// that redirects requests for intranet hostnames to another site, and if so,
// tracking what that site is (including across restarts via a pref).  For
// example, the user's ISP might convert a request for "http://query/" into a
// 302 redirect to "http://isp.domain.com/search?q=query" in order to display
// custom pages on typos, nonexistent sites, etc.
//
// We use this information in the AlternateNavURLFetcher to avoid displaying
// infobars for these cases.  Our infobars are designed to allow users to get at
// intranet sites when they were erroneously taken to a search result page.  In
// these cases, however, users would be shown a confusing and useless infobar
// when they really did mean to do a search.
//
// Consumers should call RedirectOrigin(), which is guaranteed to synchronously
// return a value at all times (even during startup or in unittest mode).  If no
// redirection is in place, the returned GURL will be empty.
class IntranetRedirectDetector
    : public content::URLFetcherDelegate,
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

  static void RegisterPrefs(PrefService* prefs);

  // The number of characters the fetcher will use for its randomly-generated
  // hostnames.
  static const size_t kNumCharsInHostnames;

 private:
  typedef std::set<net::URLFetcher*> Fetchers;

  // Called when the seven second startup sleep or the one second network
  // switch sleep has finished.  Runs any pending fetch.
  void FinishSleep();

  // content::URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver
  virtual void OnIPAddressChanged() OVERRIDE;

  GURL redirect_origin_;
  base::WeakPtrFactory<IntranetRedirectDetector> weak_factory_;
  Fetchers fetchers_;
  std::vector<GURL> resulting_origins_;
  bool in_sleep_;  // True if we're in the seven-second "no fetching" period
                   // that begins at browser start, or the one-second "no
                   // fetching" period that begins after network switches.

  DISALLOW_COPY_AND_ASSIGN(IntranetRedirectDetector);
};

// This is for use in testing, where we don't want our fetches to actually go
// over the network.  It captures the requests and causes them to fail.
class IntranetRedirectHostResolverProc : public net::HostResolverProc {
 public:
  explicit IntranetRedirectHostResolverProc(net::HostResolverProc* previous);

  virtual int Resolve(const std::string& host,
                      net::AddressFamily address_family,
                      net::HostResolverFlags host_resolver_flags,
                      net::AddressList* addrlist,
                      int* os_error) OVERRIDE;

 private:
  virtual ~IntranetRedirectHostResolverProc() {}
};

#endif  // CHROME_BROWSER_INTRANET_REDIRECT_DETECTOR_H_
