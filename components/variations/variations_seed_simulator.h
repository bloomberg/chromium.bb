// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"

namespace chrome_variations {

class ProcessedStudy;

// VariationsSeedSimulator simulates the result of creating a set of studies
// and detecting which studies would result in group changes.
class VariationsSeedSimulator {
 public:
  // Creates the simulator with the given entropy |provider|.
  explicit VariationsSeedSimulator(
      const base::FieldTrial::EntropyProvider& provider);
  virtual ~VariationsSeedSimulator();

  // Computes differences between the current process' field trial state and
  // the result of evaluating the |processed_studies| list. It is expected that
  // |processed_studies| have already been filtered and only contain studies
  // that apply to the configuration being simulated. Returns a lower bound on
  // the number of studies that are expected to change groups (lower bound due
  // to session randomized studies).
  int ComputeDifferences(
      const std::vector<ProcessedStudy>& processed_studies);

 private:
  // For the given |processed_study| with PERMANENT consistency, simulates group
  // assignment and returns true if the result differs from that study's group
  // in the current process.
  bool PermanentStudyGroupChanged(const ProcessedStudy& processed_study,
                                  const std::string& selected_group);

  // For the given |processed_study| with SESSION consistency, determines if
  // there are enough changes in the study config that restarting will result
  // in a guaranteed different group assignment (or different params).
  bool SessionStudyGroupChanged(const ProcessedStudy& filtered_study,
                                const std::string& selected_group);

  const base::FieldTrial::EntropyProvider& entropy_provider_;

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedSimulator);
};

}  // namespace chrome_variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_SIMULATOR_H_
