// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_PERF_TEST_UTILS_H_
#define CHROME_BROWSER_VR_TEST_PERF_TEST_UTILS_H_

#include <string>

#include "cc/base/lap_timer.h"

namespace vr {

void PrintResults(const std::string& suite_name,
                  const std::string& test_name,
                  cc::LapTimer* timer);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_PERF_TEST_UTILS_H_
