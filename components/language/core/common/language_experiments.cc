// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_experiments.h"

namespace language {

const base::Feature kUseHeuristicLanguageModel{
    "UseHeuristicLanguageModel", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOverrideTranslateTriggerInIndia{
    "OverrideTranslateTriggerInIndia", base::FEATURE_DISABLED_BY_DEFAULT};

OverrideLanguageModel GetOverrideLanguageModel() {
  if (base::FeatureList::IsEnabled(kUseHeuristicLanguageModel))
    return OverrideLanguageModel::HEURISTIC;
  // TODO(crbug.com/840367): Take the kOverrideTranslateTriggerInIndia params
  // into account to decide which model to use.
  return OverrideLanguageModel::DEFAULT;
}

}  // namespace language
