// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
#define COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_

#include "base/metrics/field_trial.h"

namespace chrome_variations {

class Study;

// Wrapper over Study with extra information computed during pre-processing,
// such as whether the study is expired and its total probability.
struct ProcessedStudy {
  ProcessedStudy(const Study* study,
                 base::FieldTrial::Probability total_probability,
                 bool is_expired);
  ~ProcessedStudy();

  // Corresponding Study object. Weak reference.
  const Study* study;

  // Computed total group probability for the study.
  base::FieldTrial::Probability total_probability;

  // Whether the study is expired.
  bool is_expired;
};

}  // namespace chrome_variations

#endif  // COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
