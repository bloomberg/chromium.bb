// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_

#include "base/feature_list.h"

namespace language {

// The feature that enables the heuristic model of user language. If disabled,
// the baseline model is used instead.
extern const base::Feature kUseHeuristicLanguageModel;

// This feature controls the activation of the experiment to trigger Translate
// in India on English pages independent of the user's UI language. The params
// associated with the experiment dictate which model is used to determine the
// target language. This can in turn be overriden by the Heuristic Model
// experiment.
extern const base::Feature kOverrideTranslateTriggerInIndia;

enum class OverrideLanguageModel {
  DEFAULT,
  HEURISTIC,
  GEO,
};

OverrideLanguageModel GetOverrideLanguageModel();

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_EXPERIMENTS_H_
