// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auto_launch_trial.h"

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"

const char kAutoLaunchTrialName[] = "AutoLaunchExperiment";
const char kAutoLaunchTrialAutoLaunchGroup[] = "AutoLaunching";
const char kAutoLaunchTrialControlGroup[] = "NotAutoLaunching";

namespace auto_launch_trial {

bool IsInAutoLaunchGroup() {
  return base::FieldTrialList::TrialExists(kAutoLaunchTrialName) &&
         base::FieldTrialList::Find(kAutoLaunchTrialName)->group_name()
             == kAutoLaunchTrialAutoLaunchGroup;
}

bool IsInExperimentGroup(const std::string& brand_code) {
  return LowerCaseEqualsASCII(brand_code, "rngp");
}

bool IsInControlGroup(const std::string& brand_code) {
  return LowerCaseEqualsASCII(brand_code, "rngq");
}

void UpdateToggleAutoLaunchMetric(bool auto_launch) {
  UMA_HISTOGRAM_ENUMERATION(
      base::FieldTrial::MakeName("ToggleAutoLaunch", kAutoLaunchTrialName),
      auto_launch ? 1 : 0, 2);
}

void UpdateInfobarResponseMetric(InfobarMetricResponse response) {
  UMA_HISTOGRAM_ENUMERATION(
      base::FieldTrial::MakeName("InfobarRepsonse", kAutoLaunchTrialName),
      response, 3);
}

void UpdateInfobarShownMetric() {
  UMA_HISTOGRAM_COUNTS(
      base::FieldTrial::MakeName("InfobarShown", kAutoLaunchTrialName), 1);
}

}  // namespace auto_launch_trial
