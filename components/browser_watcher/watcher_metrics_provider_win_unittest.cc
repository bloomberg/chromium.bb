// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <cstdlib>

#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

const wchar_t kRegistryPath[] = L"Software\\WatcherMetricsProviderWinTest";

class WatcherMetricsProviderWinTest : public testing::Test {
 public:
  typedef testing::Test Super;

  virtual void SetUp() override {
    Super::SetUp();

    override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
  }

  void AddProcessExitCode(bool use_own_pid, int exit_code) {
    int pid = 0;
    if (use_own_pid) {
      pid = base::GetCurrentProcId();
    } else {
      // Make sure not to accidentally collide with own pid.
      do {
        pid = rand();
      } while (pid == static_cast<int>(base::GetCurrentProcId()));
    }

    base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE);

    // Make up a unique key, starting with the given pid.
    base::string16 key_name(base::StringPrintf(L"%d-%d", pid, rand()));

    // Write the exit code to registry.
    ULONG result = key.WriteValue(key_name.c_str(), exit_code);
    ASSERT_EQ(result, ERROR_SUCCESS);
  }

  size_t ExitCodeRegistryPathValueCount() {
    base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
    return key.GetValueCount();
  }

 protected:
  registry_util::RegistryOverrideManager override_manager_;
  base::HistogramTester histogram_tester_;
};

}  // namespace

TEST_F(WatcherMetricsProviderWinTest, RecordsStabilityHistogram) {
  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(false, 0);

  // Record a single failure.
  AddProcessExitCode(false, 100);

  WatcherMetricsProviderWin provider(kRegistryPath);

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectBucketCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);
  histogram_tester_.ExpectBucketCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 100, 1);
  histogram_tester_.ExpectTotalCount(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 12);

  // Verify that the reported values are gone.
  EXPECT_EQ(ExitCodeRegistryPathValueCount(), 0);
}

TEST_F(WatcherMetricsProviderWinTest, DoesNotReportOwnProcessId) {
  // Record multiple success exits.
  for (size_t i = 0; i < 11; ++i)
    AddProcessExitCode(i, 0);

  // Record own process as STILL_ACTIVE.
  AddProcessExitCode(true, STILL_ACTIVE);

  WatcherMetricsProviderWin provider(kRegistryPath);

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectUniqueSample(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);

  // Verify that the reported values are gone.
  EXPECT_EQ(ExitCodeRegistryPathValueCount(), 1);
}

}  // namespace browser_watcher
