// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/experiments.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"

namespace dom_distiller {
DistillerHeuristicsType GetDistillerHeuristicsType() {
  // Get the field trial name first to ensure the experiment is initialized.
  const std::string group_name =
      base::FieldTrialList::FindFullName("ReaderModeUI");
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableReaderModeOGArticleHeuristics) ||
      group_name == "OGArticle" || group_name == "ForcedOGArticle") {
    return DistillerHeuristicsType::OG_ARTICLE;
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableReaderModeAdaBoostHeuristics) ||
             group_name == "AdaBoost" || group_name == "ForcedAdaBoost") {
    return DistillerHeuristicsType::ADABOOST_MODEL;
  }
  return DistillerHeuristicsType::NONE;
}
}
