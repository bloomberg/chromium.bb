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
#include "components/browser_watcher/exit_funnel_win.h"
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

  void AddExitFunnelEvent(int pid, const base::char16* name, int64 value) {
    base::string16 key_name =
        base::StringPrintf(L"%ls\\%d-%d", kRegistryPath, pid, pid);

    base::win::RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_WRITE);
    ASSERT_EQ(key.WriteValue(name, &value, sizeof(value), REG_QWORD),
              ERROR_SUCCESS);
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

  WatcherMetricsProviderWin provider(kRegistryPath, true);

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

  WatcherMetricsProviderWin provider(kRegistryPath, true);

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectUniqueSample(
        WatcherMetricsProviderWin::kBrowserExitCodeHistogramName, 0, 11);

  // Verify that the reported values are gone.
  EXPECT_EQ(ExitCodeRegistryPathValueCount(), 1);
}

TEST_F(WatcherMetricsProviderWinTest, RecordsOrderedExitFunnelEvents) {
  // Record an exit funnel with a given set of timings and check that the
  // ordering is correct on the reported histograms.
  // Note the recorded times are in microseconds, but the reporting is in
  // milliseconds, hence the times 1000.
  AddExitFunnelEvent(100, L"One", 1000 * 1000);
  AddExitFunnelEvent(100, L"Two", 1010 * 1000);
  AddExitFunnelEvent(100, L"Three", 990 * 1000);

  WatcherMetricsProviderWin provider(kRegistryPath, true);

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectUniqueSample("Stability.ExitFunnel.Three", 0, 1);
  histogram_tester_.ExpectUniqueSample("Stability.ExitFunnel.One", 10, 1);
  histogram_tester_.ExpectUniqueSample("Stability.ExitFunnel.Two", 20, 1);

  // Make sure the subkey is deleted on reporting.
  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, kRegistryPath);
  ASSERT_EQ(it.SubkeyCount(), 0);
}

TEST_F(WatcherMetricsProviderWinTest, ReadsExitFunnelWrites) {
  // Test that the metrics provider picks up the writes from the funnel.
  ExitFunnel funnel;

  // Events against our own process should not get reported.
  ASSERT_TRUE(funnel.Init(kRegistryPath, base::GetCurrentProcessHandle()));
  ASSERT_TRUE(funnel.RecordEvent(L"Forgetaboutit"));

  // Reset the funnel to a pseudo process. The PID 4 is the system process,
  // which tests can hopefully never open.
  ASSERT_TRUE(funnel.InitImpl(kRegistryPath, 4, base::Time::Now()));

  // Each named event can only exist in a single copy.
  ASSERT_TRUE(funnel.RecordEvent(L"One"));
  ASSERT_TRUE(funnel.RecordEvent(L"One"));
  ASSERT_TRUE(funnel.RecordEvent(L"One"));
  ASSERT_TRUE(funnel.RecordEvent(L"Two"));
  ASSERT_TRUE(funnel.RecordEvent(L"Three"));

  WatcherMetricsProviderWin provider(kRegistryPath, true);

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectTotalCount("Stability.ExitFunnel.One", 1);
  histogram_tester_.ExpectTotalCount("Stability.ExitFunnel.Two", 1);
  histogram_tester_.ExpectTotalCount("Stability.ExitFunnel.Three", 1);

  // Make sure the subkey for the pseudo process has been deleted on reporting.
  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, kRegistryPath);
  ASSERT_EQ(it.SubkeyCount(), 1);
}

TEST_F(WatcherMetricsProviderWinTest, ClearsExitFunnelWriteWhenNotReporting) {
  // Tests that the metrics provider cleans up, but doesn't report exit funnels
  // when funnel reporting is quenched.
  ExitFunnel funnel;

  // Events against our own process should not get reported.
  ASSERT_TRUE(funnel.Init(kRegistryPath, base::GetCurrentProcessHandle()));
  ASSERT_TRUE(funnel.RecordEvent(L"Forgetaboutit"));

  // Reset the funnel to a pseudo process. The PID 4 is the system process,
  // which tests can hopefully never open.
  ASSERT_TRUE(funnel.InitImpl(kRegistryPath, 4, base::Time::Now()));

  // Each named event can only exist in a single copy.
  ASSERT_TRUE(funnel.RecordEvent(L"One"));
  ASSERT_TRUE(funnel.RecordEvent(L"One"));
  ASSERT_TRUE(funnel.RecordEvent(L"One"));
  ASSERT_TRUE(funnel.RecordEvent(L"Two"));
  ASSERT_TRUE(funnel.RecordEvent(L"Three"));

  // Turn off exit funnel reporting.
  WatcherMetricsProviderWin provider(kRegistryPath, false);

  provider.ProvideStabilityMetrics(NULL);
  histogram_tester_.ExpectTotalCount("Stability.ExitFunnel.One", 0);
  histogram_tester_.ExpectTotalCount("Stability.ExitFunnel.Two", 0);
  histogram_tester_.ExpectTotalCount("Stability.ExitFunnel.Three", 0);

  // Make sure the subkey for the pseudo process has been deleted on reporting.
  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, kRegistryPath);
  ASSERT_EQ(it.SubkeyCount(), 1);
}

}  // namespace browser_watcher
