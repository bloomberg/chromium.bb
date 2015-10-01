// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/site_isolation_policy.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

class SiteIsolationWhitelist {
 public:
  SiteIsolationWhitelist() {
    // Call into the embedder to get the list of isolated schemes from the
    // command-line configuration.
    //
    // TODO(nick): https://crbug.com/133403 Because the AtExitManager doesn't
    // run between unit tests, it's not possible for unit tests to safely alter
    // the isolated schemes here.
    GetContentClient()->AddIsolatedSchemes(&isolated_schemes_);
  }
  ~SiteIsolationWhitelist() {}

  const std::set<std::string>& isolated_schemes() const {
    return isolated_schemes_;
  }

  bool should_isolate_all_sites() const {
    // TODO(nick): https://crbug.com/133403 We ought to cache the value of this
    // switch here, but cannot because the AtExitManager doesn't run between
    // unit tests.
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kSitePerProcess);
  }

 private:
  std::set<std::string> isolated_schemes_;
  DISALLOW_COPY_AND_ASSIGN(SiteIsolationWhitelist);
};

base::LazyInstance<SiteIsolationWhitelist> g_site_isolation_whitelist =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
bool SiteIsolationPolicy::AreCrossProcessFramesPossible() {
  const SiteIsolationWhitelist& whitelist = g_site_isolation_whitelist.Get();
  return whitelist.should_isolate_all_sites() ||
         !whitelist.isolated_schemes().empty();
}

// static
bool SiteIsolationPolicy::DoesSiteRequireDedicatedProcess(
    const GURL& effective_url) {
  const SiteIsolationWhitelist& whitelist = g_site_isolation_whitelist.Get();
  return whitelist.should_isolate_all_sites() ||
         (!whitelist.isolated_schemes().empty() &&
          whitelist.isolated_schemes().count(effective_url.scheme()));
}

// static
bool SiteIsolationPolicy::UseSubframeNavigationEntries() {
  // Enable the new navigation history behavior if any manner of site isolation
  // is active.
  return AreCrossProcessFramesPossible();
}

// static
bool SiteIsolationPolicy::IsSwappedOutStateForbidden() {
  return AreCrossProcessFramesPossible();
}

// static
bool SiteIsolationPolicy::IsolateAllSitesForTesting() {
  // TODO(nick): Re-enable once https://crbug.com/133403 is fixed.
  // if (!(g_site_isolation_whitelist == nullptr)) return false;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSitePerProcess);
  return true;
}

}  // namespace content
