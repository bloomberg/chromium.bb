// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TIME_TICKS_EXPERIMENT_WIN_H_
#define CHROME_BROWSER_METRICS_TIME_TICKS_EXPERIMENT_WIN_H_

#include "build/build_config.h"

#if defined(OS_WIN)

namespace chrome {

// Runs an experiment once per session upon successful upload of UMA. Determines
// if QueryPerformanceCounter() is safe to use generally on this system. Ensures
// that the resolution is better than 1ms, returns quickly, and is monotonically
// increasing. Records the results in UMA for the next upload.
void CollectTimeTicksStats();

}  // namespace chrome

#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_METRICS_TIME_TICKS_EXPERIMENT_WIN_H_
