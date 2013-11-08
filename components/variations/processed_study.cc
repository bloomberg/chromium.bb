// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include "components/variations/proto/study.pb.h"

namespace chrome_variations {

ProcessedStudy::ProcessedStudy(const Study* study,
                               base::FieldTrial::Probability total_probability,
                               bool is_expired)
    : study(study),
      total_probability(total_probability),
      is_expired(is_expired) {
}

ProcessedStudy::~ProcessedStudy() {
}

}  // namespace chrome_variations
