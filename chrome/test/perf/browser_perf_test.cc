// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/perf/browser_perf_test.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/perf/perf_test.h"

BrowserPerfTest::BrowserPerfTest() {
}

BrowserPerfTest::~BrowserPerfTest() {
}

void BrowserPerfTest::SetUpCommandLine(CommandLine* command_line) {
  // Reduce performance test variance by disabling background networking.
  command_line->AppendSwitch(switches::kDisableBackgroundNetworking);
}

void BrowserPerfTest::PrintIOPerfInfo(const std::string& test_name) {
  base::ProcessId browser_pid = base::GetCurrentProcId();
  ChromeProcessList chrome_processes(GetRunningChromeProcesses(browser_pid));
  perf_test::PrintIOPerfInfo(test_name, chrome_processes, browser_pid);
}

void BrowserPerfTest::PrintMemoryUsageInfo(const std::string& test_name) {
  base::ProcessId browser_pid = base::GetCurrentProcId();
  ChromeProcessList chrome_processes(GetRunningChromeProcesses(browser_pid));
  perf_test::PrintMemoryUsageInfo(test_name, chrome_processes, browser_pid);
}
