// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_HASHES_H_
#define COMPONENTS_METRICS_METRICS_HASHES_H_

#include <string>

#include "base/basictypes.h"

namespace metrics {

// Computes a uint64 hash of a given string based on its MD5 hash. Suitable for
// metric names.
uint64 HashMetricName(const std::string& name);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_HASHES_H_
