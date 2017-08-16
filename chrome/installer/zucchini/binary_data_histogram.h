// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_BINARY_DATA_HISTOGRAM_H_
#define CHROME_INSTALLER_ZUCCHINI_BINARY_DATA_HISTOGRAM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "chrome/installer/zucchini/buffer_view.h"

namespace zucchini {

// A class to compute similarity score between binary data. The heuristic here
// preprocesses input data to a size-65536 histogram, counting the frequency of
// consecutive 2-byte sequences. Therefore data with lengths < 2 are considered
// invalid -- but this is okay for Zucchini's use case.
class BinaryDataHistogram {
 public:
  BinaryDataHistogram();
  ~BinaryDataHistogram();

  // Attempts to compute the histogram, returns true iff successful.
  bool Compute(ConstBufferView region);

  bool IsValid() const { return static_cast<bool>(histogram_); }

  // Returns distance to another histogram (heuristics). If two binaries are
  // identical then their histogram distance is 0. However, the converse is not
  // true in general. For example, "aba" and "bab" are different, but their
  // histogram distance is 0 (both histograms are {"ab": 1, "ba": 1}).
  double Distance(const BinaryDataHistogram& other) const;

 private:
  enum { kNumBins = 1 << (sizeof(uint16_t) * 8) };
  static_assert(kNumBins == 65536, "Incorrect constant computation.");

  // Size, in bytes, of the data over which the histogram was computed.
  size_t size_ = 0;

  // 2^16 buckets holding counts of all 2-byte sequences in the data. The counts
  // are stored as signed values to simplify computing the distance between two
  // histograms.
  std::unique_ptr<int32_t[]> histogram_;

  DISALLOW_COPY_AND_ASSIGN(BinaryDataHistogram);
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_BINARY_DATA_HISTOGRAM_H_
