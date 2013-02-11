// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_perf_test.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/perf/perf_test.h"

void UIPerfTest::SetLaunchSwitches() {
  UITestBase::SetLaunchSwitches();

  // Reduce performance test variance by disabling background networking.
  launch_arguments_.AppendSwitch(switches::kDisableBackgroundNetworking);

  // We don't want tests to slow down mysteriously when we update the minimum
  // plugin version.
  launch_arguments_.AppendSwitch(switches::kAllowOutdatedPlugins);
}

void UIPerfTest::PrintIOPerfInfo(const char* test_name) {
  ChromeProcessList chrome_processes(
      GetRunningChromeProcesses(browser_process_id()));
  perf_test::PrintIOPerfInfo(test_name, chrome_processes,
                             browser_process_id());
}

void UIPerfTest::PrintMemoryUsageInfo(const char* test_name) {
  ChromeProcessList chrome_processes(
      GetRunningChromeProcesses(browser_process_id()));
  perf_test::PrintMemoryUsageInfo(test_name, chrome_processes,
                                  browser_process_id());
}

void UIPerfTest::UseReferenceBuild() {
  base::FilePath dir;
  PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
  dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
  dir = dir.AppendASCII("chrome_win");
#elif defined(OS_LINUX)
  dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
  dir = dir.AppendASCII("chrome_mac");
#endif
  launch_arguments_.AppendSwitch(switches::kEnableChromiumBranding);
  SetBrowserDirectory(dir);
}
