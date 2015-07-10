// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_working_set_snapshot.h"

#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

using PrivateWorkingSetSnapshotWinTest = testing::Test;

TEST_F(PrivateWorkingSetSnapshotWinTest, FindPidSelfTest) {
  // The Pdh APIs are supported on Windows XP and above, but the "Working Set -
  // Private" counter that PrivateWorkingSetSnapshot depends on is not defined
  // until Windows Vista and is not reliable until Windows 7. Early-out to avoid
  // test failure.
  if (base::win::GetVersion() <= base::win::VERSION_VISTA)
    return;

  // Sample this process.
  base::ProcessId pid = base::GetCurrentProcId();

  PrivateWorkingSetSnapshot private_ws_snapshot;

  private_ws_snapshot.AddToMonitorList("unit_tests");
  private_ws_snapshot.Sample();

  size_t private_ws = private_ws_snapshot.GetPrivateWorkingSet(pid);
  if (private_ws == 0) {
    // The Pdh APIs which PrivateWorkingSetSnapshot depends on are flaky and
    // sometimes (~5% of the time on win_chromium_rel_ng) give back a result of
    // zero. This is handled seamlessly in task manager by falling back to the
    // old calculations so it is not a critical failure. Checking for this case
    // and skipping the test stops flaky-test failures.
    return;
  }

  // Private working set is difficult to predict but should be at least several
  // MB. Initial tests show a value of 19+ MB depending on how many tests and
  // processes are used. Anomalously small or large values would warrant
  // investigation.
  EXPECT_GT(private_ws, 2000000u);
  // Check that the WS is less than 1500 MB. This is set very high to reduce the
  // chance that unrelated changes could ever make this fail. This mostly just
  // checks against some uncaught error that might return 0xFFFFFFFF. When run
  // under Dr Memory the private working set was seen to be about 850 MB, which
  // is why such a high threshold has been chosen.
  EXPECT_LT(private_ws, 1500000000u);

  // Allocate and touch a large block of memory (vector's constructor will zero
  // every entry). This will increase the private working set.
  const size_t alloc_size = 10000000;
  std::vector<char> big_memory(alloc_size);

  size_t private_ws2 = private_ws_snapshot.GetPrivateWorkingSet(pid);
  EXPECT_EQ(private_ws, private_ws2) << "GetPrivateWorkingSet should be "
                                        "consistent until the next call to "
                                        "Sample()";

  private_ws_snapshot.Sample();
  size_t private_ws3 = private_ws_snapshot.GetPrivateWorkingSet(pid);
  EXPECT_GT(private_ws3, private_ws2 + alloc_size / 2)
      << "GetPrivateWorkingSet should increase as we allocate more memory";
}
