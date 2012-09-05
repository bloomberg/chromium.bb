// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_METRICS_UTIL_H_
#define CHROME_COMMON_METRICS_METRICS_UTIL_H_

#include <string>

#include "base/basictypes.h"

namespace metrics {

// Computes a uint32 hash of a given string based on its SHA1 hash. Suitable for
// uniquely identifying field trial names and group names.
uint32 HashName(const std::string& name);

}  // namespace metrics

#endif  // CHROME_COMMON_METRICS_METRICS_UTIL_H_
