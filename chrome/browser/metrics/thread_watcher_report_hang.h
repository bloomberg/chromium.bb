// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_THREAD_WATCHER_REPORT_HANG_H_
#define CHROME_BROWSER_METRICS_THREAD_WATCHER_REPORT_HANG_H_

#include "base/compiler_specific.h"

namespace metrics {

// This function makes it possible to tell from the callstack why startup is
// taking too long.
NOINLINE void StartupHang();

// This function makes it possible to tell from the callstack why shutdown is
// taking too long.
NOINLINE void ShutdownHang();

// This function makes it possible to tell from the callstack alone what thread
// was unresponsive.
NOINLINE void CrashBecauseThreadWasUnresponsive(int thread_id);

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_THREAD_WATCHER_REPORT_HANG_H_
