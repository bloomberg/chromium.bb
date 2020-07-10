// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_report_flags.h"

#include "base/metrics/field_trial_params.h"

namespace crash_report {

const base::Feature kBreakpadNoDelayInitialUpload{
    "BreakpadNoDelayInitialUpload", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDetectMainThreadFreeze{"DetectMainThreadFreeze",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// The different timeout value for kDetectMainThreadFreeze.
const char kDetectMainThreadFreezeParameterName[] = "timeout";
const char kDetectMainThreadFreezeParameter3s[] = "3s";
const char kDetectMainThreadFreezeParameter5s[] = "5s";
const char kDetectMainThreadFreezeParameter7s[] = "7s";
const char kDetectMainThreadFreezeParameter9s[] = "9s";

int TimeoutForMainThreadFreezeDetection() {
  if (!base::FeatureList::IsEnabled(kDetectMainThreadFreeze)) {
    return 0;
  }
  std::string parameter = base::GetFieldTrialParamValueByFeature(
      kDetectMainThreadFreeze, kDetectMainThreadFreezeParameterName);
  if (parameter == kDetectMainThreadFreezeParameter3s) {
    return 3;
  }
  if (parameter == kDetectMainThreadFreezeParameter5s) {
    return 5;
  }
  if (parameter == kDetectMainThreadFreezeParameter7s) {
    return 7;
  }
  if (parameter == kDetectMainThreadFreezeParameter9s) {
    return 9;
  }
  return 0;
}

}  // namespace crash_report
