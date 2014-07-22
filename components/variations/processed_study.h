// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
#define COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"

namespace chrome_variations {

class Study;

// Wrapper over Study with extra information computed during pre-processing,
// such as whether the study is expired and its total probability.
class ProcessedStudy {
 public:
  ProcessedStudy();
  ~ProcessedStudy();

  bool Init(const Study* study, bool is_expired);

  const Study* study() const { return study_; }

  base::FieldTrial::Probability total_probability() const {
    return total_probability_;
  }

  bool is_expired() const { return is_expired_; }

  // Gets the index of the experiment with the given |name|. Returns -1 if no
  // experiment is found.
  int GetExperimentIndexByName(const std::string& name) const;

  static bool ValidateAndAppendStudy(
      const Study* study,
      bool is_expired,
      std::vector<ProcessedStudy>* processed_studies);

 private:
  // Corresponding Study object. Weak reference.
  const Study* study_;

  // Computed total group probability for the study.
  base::FieldTrial::Probability total_probability_;

  // Whether the study is expired.
  bool is_expired_;
};

}  // namespace chrome_variations

#endif  // COMPONENTS_VARIATIONS_PROCESSED_STUDY_H_
