// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTO_LAUNCH_TRIAL_H_
#define CHROME_BROWSER_AUTO_LAUNCH_TRIAL_H_

#include <string>

// Strings used with the "auto launching Chrome at computer startup" trial.  If
// the field trial is running then...
// base::FieldTrialList::TrialExists(kAutoLaunchTrial_Name) returns true.
//
// The field trial consists of two groups of users: those that auto-launch
// Chrome at startup and those that don't.  The group_name() of the field
// trial object is used to determine the group that the user belongs to.
//
// The field trial is setup in ChromeBrowserMainParts::AutoLaunchFieldTrial()
// based on the user's brand code:
//
//   - brand RNGP auto launches Chrome on computer startup.
//   - brand RNGQ does not.
//   - any other brand code does whatever comes natural to it.

extern const char kAutoLaunchTrialName[];
extern const char kAutoLaunchTrialAutoLaunchGroup[];
extern const char kAutoLaunchTrialControlGroup[];

namespace auto_launch_trial {

// The possible responses for the auto-launch infobar.
enum InfobarMetricResponse {
  INFOBAR_CUT_IT_OUT = 0,
  INFOBAR_OK,
  INFOBAR_IGNORE,
};

// Whether the auto-launch experiment is active and the user is part of it.
bool IsInAutoLaunchGroup();

// Whether the brand is part of the experiment group for auto-launch.
bool IsInExperimentGroup(const std::string& brand_code);

// Whether the brand is part of the control group for auto-launch.
bool IsInControlGroup(const std::string& brand_code);

}  // namespace auto_launch_trial

#endif  // CHROME_BROWSER_AUTO_LAUNCH_TRIAL_H_
