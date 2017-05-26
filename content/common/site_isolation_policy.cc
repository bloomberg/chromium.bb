// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/site_isolation_policy.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

// static
bool SiteIsolationPolicy::AreCrossProcessFramesPossible() {
// Before turning this on for Android, input event routing needs to be
// completed there, and perf regressions in https://crbug.com/690229 need to be
// investigated.
#if defined(OS_ANDROID)
  return UseDedicatedProcessesForAllSites() ||
         IsTopDocumentIsolationEnabled() || AreIsolatedOriginsEnabled() ||
         GetContentClient()->IsSupplementarySiteIsolationModeEnabled() ||
         base::FeatureList::IsEnabled(::features::kGuestViewCrossProcessFrames);
#else
  return true;
#endif
}

// static
bool SiteIsolationPolicy::UseDedicatedProcessesForAllSites() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
}

// static
bool SiteIsolationPolicy::IsTopDocumentIsolationEnabled() {
  // --site-per-process trumps --top-document-isolation.
  if (UseDedicatedProcessesForAllSites())
    return false;

  return base::FeatureList::IsEnabled(::features::kTopDocumentIsolation);
}

// static
bool SiteIsolationPolicy::AreIsolatedOriginsEnabled() {
  // TODO(alexmos): This currently assumes that isolated origins are only added
  // via the command-line switch, which may not be true in the future.  Remove
  // this function when AreCrossProcessFramesPossible becomes true on Android
  // above.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIsolateOrigins);
}

}  // namespace content
