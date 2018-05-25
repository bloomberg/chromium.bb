// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_experiments.h"

#include <map>
#include <string>

#include "base/metrics/field_trial_params.h"

namespace language {

const base::Feature kUseHeuristicLanguageModel{
    "UseHeuristicLanguageModel", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOverrideTranslateTriggerInIndia{
    "OverrideTranslateTriggerInIndia", base::FEATURE_DISABLED_BY_DEFAULT};
const char kOverrideModelKey[] = "override_model";
const char kEnforceRankerKey[] = "enforce_ranker";
const char kOverrideModelHeuristicValue[] = "heuristic";
const char kOverrideModelGeoValue[] = "geo";

OverrideLanguageModel GetOverrideLanguageModel() {
  std::map<std::string, std::string> params;
  bool should_override_model = base::GetFieldTrialParamsByFeature(
      kOverrideTranslateTriggerInIndia, &params);

  if (base::FeatureList::IsEnabled(kUseHeuristicLanguageModel) ||
      (should_override_model &&
       params[kOverrideModelKey] == kOverrideModelHeuristicValue)) {
    return OverrideLanguageModel::HEURISTIC;
  }

  if (should_override_model &&
      params[kOverrideModelKey] == kOverrideModelGeoValue) {
    return OverrideLanguageModel::GEO;
  }

  return OverrideLanguageModel::DEFAULT;
}

bool ShouldForceTriggerTranslateOnEnglishPages() {
  return base::FeatureList::IsEnabled(kOverrideTranslateTriggerInIndia);
}

bool ShouldPreventRankerEnforcementInIndia() {
  std::map<std::string, std::string> params;
  return base::GetFieldTrialParamsByFeature(kOverrideTranslateTriggerInIndia,
                                            &params) &&
         params[kEnforceRankerKey] == "false";
}

}  // namespace language
