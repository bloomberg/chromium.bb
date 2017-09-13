// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/expired_histograms_checker.h"

#include <algorithm>

namespace metrics {

ExpiredHistogramsChecker::ExpiredHistogramsChecker(const uint64_t* array,
                                                   size_t size)
    : array_(array), size_(size) {}

ExpiredHistogramsChecker::~ExpiredHistogramsChecker() {}

bool ExpiredHistogramsChecker::ShouldRecord(uint64_t histogram_hash) const {
  return !std::binary_search(array_, array_ + size_, histogram_hash);
}

}  // namespace metrics
