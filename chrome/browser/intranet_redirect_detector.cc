// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intranet_redirect_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

IntranetRedirectDetector::IntranetRedirectDetector()
    : redirect_origin_(g_browser_process->local_state()->GetString(
          prefs::kLastKnownIntranetRedirectOrigin)),
      in_sleep_(true),
      weak_ptr_factory_(this) {
  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay seven seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  static const int kStartFetchDelaySeconds = 7;
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&IntranetRedirectDetector::FinishSleep,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kStartFetchDelaySeconds));

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
}

IntranetRedirectDetector::~IntranetRedirectDetector() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  STLDeleteElements(&fetchers_);
}

// static
GURL IntranetRedirectDetector::RedirectOrigin() {
  const IntranetRedirectDetector* const detector =
      g_browser_process->intranet_redirect_detector();
  return detector ? detector->redirect_origin_ : GURL();
}

// static
void IntranetRedirectDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kLastKnownIntranetRedirectOrigin,
                               std::string());
}

void IntranetRedirectDetector::FinishSleep() {
  in_sleep_ = false;

  // If another fetch operation is still running, cancel it.
  STLDeleteElements(&fetchers_);
  resulting_origins_.clear();

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBackgroundNetworking))
    return;

  DCHECK(fetchers_.empty() && resulting_origins_.empty());

  // Start three fetchers on random hostnames.
  for (size_t i = 0; i < 3; ++i) {
    std::string url_string("http://");
    // We generate a random hostname with between 7 and 15 characters.
    const int num_chars = base::RandInt(7, 15);
    for (int j = 0; j < num_chars; ++j)
      url_string += ('a' + base::RandInt(0, 'z' - 'a'));
    GURL random_url(url_string + '/');
    net::URLFetcher* fetcher = net::URLFetcher::Create(
        random_url, net::URLFetcher::HEAD, this);
    // We don't want these fetches to affect existing state in the profile.
    fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                          net::LOAD_DO_NOT_SAVE_COOKIES |
                          net::LOAD_DO_NOT_SEND_COOKIES);
    fetcher->SetRequestContext(g_browser_process->system_request_context());
    fetcher->Start();
    fetchers_.insert(fetcher);
  }
}

void IntranetRedirectDetector::OnURLFetchComplete(
    const net::URLFetcher* source) {
  // Delete the fetcher on this function's exit.
  Fetchers::iterator fetcher = fetchers_.find(
      const_cast<net::URLFetcher*>(source));
  DCHECK(fetcher != fetchers_.end());
  scoped_ptr<net::URLFetcher> clean_up_fetcher(*fetcher);
  fetchers_.erase(fetcher);

  // If any two fetches result in the same domain/host, we set the redirect
  // origin to that; otherwise we set it to nothing.
  if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200)) {
    if ((resulting_origins_.empty()) ||
        ((resulting_origins_.size() == 1) &&
         resulting_origins_.front().is_valid())) {
      resulting_origins_.push_back(GURL());
      return;
    }
    redirect_origin_ = GURL();
  } else {
    DCHECK(source->GetURL().is_valid());
    GURL origin(source->GetURL().GetOrigin());
    if (resulting_origins_.empty()) {
      resulting_origins_.push_back(origin);
      return;
    }
    if (net::registry_controlled_domains::SameDomainOrHost(
        resulting_origins_.front(),
        origin,
        net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)) {
      redirect_origin_ = origin;
      if (!fetchers_.empty()) {
        // Cancel remaining fetch, we don't need it.
        DCHECK(fetchers_.size() == 1);
        delete (*fetchers_.begin());
        fetchers_.clear();
      }
    }
    if (resulting_origins_.size() == 1) {
      resulting_origins_.push_back(origin);
      return;
    }
    DCHECK(resulting_origins_.size() == 2);
    const bool same_domain_or_host =
        net::registry_controlled_domains::SameDomainOrHost(
            resulting_origins_.back(),
            origin,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    redirect_origin_ = same_domain_or_host ? origin : GURL();
  }

  g_browser_process->local_state()->SetString(
      prefs::kLastKnownIntranetRedirectOrigin, redirect_origin_.is_valid() ?
          redirect_origin_.spec() : std::string());
}

void IntranetRedirectDetector::OnIPAddressChanged() {
  // If a request is already scheduled, do not scheduled yet another one.
  if (in_sleep_)
    return;

  // Since presumably many programs open connections after network changes,
  // delay this a little bit.
  in_sleep_ = true;
  static const int kNetworkSwitchDelayMS = 1000;
  base::MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&IntranetRedirectDetector::FinishSleep,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kNetworkSwitchDelayMS));
}
