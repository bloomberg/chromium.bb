// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_VARIATIONS_UNIFORMITY_FIELD_TRIALS_H_
#define CHROME_COMMON_METRICS_VARIATIONS_UNIFORMITY_FIELD_TRIALS_H_

#include "base/time.h"

namespace chrome_variations {

// A collection of one-time-randomized and session-randomized field trials
// intended to test the uniformity and correctness of the field trial control,
// bucketing and reporting systems.
void SetupUniformityFieldTrials(const base::Time& install_date);

}  // namespace chrome_variations

#endif  // CHROME_COMMON_METRICS_VARIATIONS_UNIFORMITY_FIELD_TRIALS_H_
