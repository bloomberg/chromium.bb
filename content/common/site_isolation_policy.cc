// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/site_isolation_policy.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool SiteIsolationPolicy::AreCrossProcessFramesPossible() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSitePerProcess) ||
         GetContentClient()->IsSupplementarySiteIsolationModeEnabled();
}

// static
bool SiteIsolationPolicy::UseDedicatedProcessesForAllSites() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

// static
bool SiteIsolationPolicy::UseSubframeNavigationEntries() {
  // Enable the new navigation history behavior if any manner of site isolation
  // is active.
  // PlzNavigate: also enable the new navigation history behavior.
  return AreCrossProcessFramesPossible() || IsBrowserSideNavigationEnabled();
}

// static
bool SiteIsolationPolicy::IsSwappedOutStateForbidden() {
  return true;
}

}  // namespace content
