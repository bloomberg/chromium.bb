// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_
#define COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/metrics/record_histogram_checker.h"

namespace metrics {

// ExpiredHistogramsChecker implements RecordHistogramChecker interface
// to avoid recording expired metrics.
class ExpiredHistogramsChecker final : public base::RecordHistogramChecker {
 public:
  // Takes sorted in nondecreasing order array of histogram hashes and its size.
  ExpiredHistogramsChecker(const uint64_t* array, size_t size);
  ~ExpiredHistogramsChecker() override;

  // Checks if the given |histogram_hash| corresponds to an expired histogram.
  bool ShouldRecord(uint64_t histogram_hash) const override;

 private:
  const uint64_t* const array_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(ExpiredHistogramsChecker);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EXPIRED_HISTOGRAMS_CHECKER_H_
