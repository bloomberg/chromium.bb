// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/site_isolation_policy.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool SiteIsolationPolicy::AreCrossProcessFramesPossible() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

// static
bool SiteIsolationPolicy::DoesSiteRequireDedicatedProcess(const GURL& gurl) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

// static
bool SiteIsolationPolicy::UseSubframeNavigationEntries() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

// static
bool SiteIsolationPolicy::IsSwappedOutStateForbidden() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

}  // namespace content
