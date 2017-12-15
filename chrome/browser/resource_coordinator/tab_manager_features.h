// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace features {

extern const base::Feature kCustomizedTabLoadTimeout;
extern const base::Feature kProactiveTabDiscarding;
extern const base::Feature kStaggeredBackgroundTabOpening;
extern const base::Feature kStaggeredBackgroundTabOpeningExperiment;

}  // namespace features

namespace resource_coordinator {

// Variations parameter names related to proactive discarding.
// See ProactiveTabDiscardsParams for details.
extern const char kProactiveTabDiscard_LowLoadedTabCountParam[];
extern const char kProactiveTabDiscard_ModerateLoadedTabsPerGbRamParam[];
extern const char kProactiveTabDiscard_HighLoadedTabCountParam[];
extern const char kProactiveTabDiscard_LowOccludedTimeoutParam[];
extern const char kProactiveTabDiscard_ModerateOccludedTimeoutParam[];
extern const char kProactiveTabDiscard_HighOccludedTimeoutParam[];

// Default values of parameters related to proactive discarding.
// See ProactiveTabDiscardsParams for details.
extern const uint32_t kProactiveTabDiscard_LowLoadedTabCountDefault;
extern const uint32_t kProactiveTabDiscard_ModerateLoadedTabsPerGbRamDefault;
extern const uint32_t kProactiveTabDiscard_HighLoadedTabCountDefault;
extern const base::TimeDelta kProactiveTabDiscard_LowOccludedTimeoutDefault;
extern const base::TimeDelta
    kProactiveTabDiscard_ModerateOccludedTimeoutDefault;
extern const base::TimeDelta kProactiveTabDiscard_HighOccludedTimeoutDefault;

// Parameters used by the proactive tab discarding feature.
//
// Proactive discarding has 5 key parameters:
//
// - min/max occluded timeouts
// - min/soft_max/hard_max loaded tab counts
//
// Proactive tab discarding decisions are made at two points in time:
//
// - when a new tab is created
// - when a max occluded timeout fires
//
// The following is a description of the initial simple proactive discarding
// logic. First, the number of loaded tabs is converted into one of 4 tab count
// states (LOW, MODERATE, HIGH, EXCESSIVE) using 3 simple thresholds.
//
// +-------+----------+---------+-----------+
// +  LOW  | MODERATE |  HIGH   | EXCESSIVE |
// +-------+----------+---------+-----------+
// 0      n_low      n_mod     n_high      +inf
//
// Depending on the tab count state, tabs are eligible for proactive discarding
// at different time tresholds, where the timeout is longer for lower tab
// count states. When in the low state the timeout is effectively infinite (no
// proactive discarding will occur), and when in the excessive state the timeout
// is zero (discarding will occur immediately).
//
// This logic is independent of urgent discarding, which may embark when things
// are sufficiently bad. Similarly, manual or extension driven discards can
// override this logic. Finally, proactive discarding can only discard occluded
// tabs, so it is always possible to have arbitrarily many visible tabs.
//
// NOTE: This is extremely simplistic, and by design. We will be using this to
// do a very simple "lightspeed" experiment to determine how much possible
// savings proactive discarding can hope to achieve.
struct ProactiveTabDiscardParams {
  // Tab count (inclusive) beyond which the state transitions to MODERATE.
  // Intended to cover the majority of simple workflows and be small enough that
  // it is very unlikely that memory pressure will be encountered with this many
  // tabs loaded.
  int low_loaded_tab_count;
  // Tab count (inclusive) beyond which the state transitions to HIGH, expressed
  // relative to system memory.
  int moderate_loaded_tab_count_per_gb;
  // Tab count (inclusive) beyond which the state transitions to EXCESSIVE.
  // Not expressed relative to system memory, as its intended to be a hard cap
  // more akin to a maximum mental model size.
  int high_loaded_tab_count;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is LOW.
  base::TimeDelta low_occluded_timeout;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is MODERATE.
  base::TimeDelta moderate_occluded_timeout;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is HIGH.
  base::TimeDelta high_occluded_timeout;
};

// Gets parameters for the proactive tab discarding feature. This does no
// parameter validation, and sets the default values if the feature is not
// enabled.
void GetProactiveTabDiscardParams(ProactiveTabDiscardParams* params);

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
