// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERF_PERF_TEST_H_
#define CHROME_TEST_PERF_PERF_TEST_H_

#include <stdio.h>
#include <string>

#include "chrome/test/base/chrome_process_util.h"

namespace perf_test {

// Prints IO performance data for use by perf graphs.
void PrintIOPerfInfo(const std::string& test_name,
                     const ChromeProcessList& chrome_processes,
                     base::ProcessId browser_pid);

void PrintIOPerfInfo(FILE* target,
                     const std::string& test_name,
                     const ChromeProcessList& chrome_processes,
                     base::ProcessId browser_pid);

std::string IOPerfInfoToString(const std::string& test_name,
                               const ChromeProcessList& chrome_processes,
                               base::ProcessId browser_pid);

// Prints memory usage data for use by perf graphs.
void PrintMemoryUsageInfo(const std::string& test_name,
                          const ChromeProcessList& chrome_processes,
                          base::ProcessId browser_pid);

void PrintMemoryUsageInfo(FILE* target,
                          const std::string& test_name,
                          const ChromeProcessList& chrome_processes,
                          base::ProcessId browser_pid);

std::string MemoryUsageInfoToString(const std::string& test_name,
                                    const ChromeProcessList& chrome_processes,
                                    base::ProcessId browser_pid);

}  // namespace perf_test

#endif  // CHROME_TEST_PERF_PERF_TEST_H_
