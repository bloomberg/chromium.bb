// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/binary_data_histogram.h"

#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace zucchini {

BinaryDataHistogram::BinaryDataHistogram() = default;

BinaryDataHistogram::~BinaryDataHistogram() = default;

bool BinaryDataHistogram::Compute(ConstBufferView region) {
  DCHECK(!histogram_);
  // Binary data with size < 2 are invalid.
  if (region.size() < sizeof(uint16_t))
    return false;
  DCHECK_LE(region.size(),
            static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  histogram_ = base::MakeUnique<int32_t[]>(kNumBins);
  size_ = region.size();
  // Number of 2-byte intervals fully contained in |region|.
  size_t bound = size_ - sizeof(uint16_t) + 1;
  for (size_t i = 0; i < bound; ++i)
    ++histogram_[region.read<uint16_t>(i)];
  return true;
}

double BinaryDataHistogram::Distance(const BinaryDataHistogram& other) const {
  DCHECK(IsValid() && other.IsValid());
  // Compute Manhattan (L1) distance between respective histograms.
  double total_diff = 0;
  for (int i = 0; i < kNumBins; ++i)
    total_diff += std::abs(histogram_[i] - other.histogram_[i]);
  // Normalize by total size, so result lies in [0, 1].
  return total_diff / (size_ + other.size_);
}

}  // namespace zucchini
