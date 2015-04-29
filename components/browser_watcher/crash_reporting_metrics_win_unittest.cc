// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/crash_reporting_metrics_win.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

const base::char16 test_root[] = L"TmpTmp";

class CrashReportingMetricsTest : public testing::Test {
 protected:
  CrashReportingMetricsTest() {}

  void SetUp() override {
    // Create a temporary key for testing
    base::win::RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(test_root);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, test_root,
                                        KEY_READ));
  }

  void TearDown() override {
    // Clean up the temporary key
    base::win::RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(test_root));
  }

  base::string16 registry_path() {
    return base::string16(test_root) + L"\\CrashReportingMetricsTest";
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashReportingMetricsTest);
};

}  // namespace

TEST_F(CrashReportingMetricsTest, BasicTest) {
  CrashReportingMetrics::Values values =
      CrashReportingMetrics(registry_path()).RetrieveAndResetMetrics();
  EXPECT_EQ(0, values.crash_dump_attempts);
  EXPECT_EQ(0, values.successful_crash_dumps);
  EXPECT_EQ(0, values.failed_crash_dumps);
  EXPECT_EQ(0, values.dump_without_crash_attempts);
  EXPECT_EQ(0, values.successful_dumps_without_crash);
  EXPECT_EQ(0, values.failed_dumps_without_crash);

  CrashReportingMetrics writer(registry_path());
  writer.RecordCrashDumpAttempt();
  writer.RecordCrashDumpAttempt();
  writer.RecordCrashDumpAttempt();
  writer.RecordDumpWithoutCrashAttempt();
  writer.RecordDumpWithoutCrashAttempt();
  writer.RecordDumpWithoutCrashAttempt();
  writer.RecordDumpWithoutCrashAttempt();

  writer.RecordCrashDumpAttemptResult(true);
  writer.RecordCrashDumpAttemptResult(false);
  writer.RecordCrashDumpAttemptResult(false);
  writer.RecordDumpWithoutCrashAttemptResult(true);
  writer.RecordDumpWithoutCrashAttemptResult(true);
  writer.RecordDumpWithoutCrashAttemptResult(true);
  writer.RecordDumpWithoutCrashAttemptResult(false);

  values = CrashReportingMetrics(registry_path()).RetrieveAndResetMetrics();
  EXPECT_EQ(3, values.crash_dump_attempts);
  EXPECT_EQ(1, values.successful_crash_dumps);
  EXPECT_EQ(2, values.failed_crash_dumps);
  EXPECT_EQ(4, values.dump_without_crash_attempts);
  EXPECT_EQ(3, values.successful_dumps_without_crash);
  EXPECT_EQ(1, values.failed_dumps_without_crash);

  values = CrashReportingMetrics(registry_path()).RetrieveAndResetMetrics();
  EXPECT_EQ(0, values.crash_dump_attempts);
  EXPECT_EQ(0, values.successful_crash_dumps);
  EXPECT_EQ(0, values.failed_crash_dumps);
  EXPECT_EQ(0, values.dump_without_crash_attempts);
  EXPECT_EQ(0, values.successful_dumps_without_crash);
  EXPECT_EQ(0, values.failed_dumps_without_crash);

  writer.RecordCrashDumpAttempt();
  writer.RecordCrashDumpAttempt();
  writer.RecordDumpWithoutCrashAttempt();
  writer.RecordDumpWithoutCrashAttempt();
  writer.RecordDumpWithoutCrashAttempt();

  writer.RecordCrashDumpAttemptResult(true);
  writer.RecordCrashDumpAttemptResult(true);
  writer.RecordDumpWithoutCrashAttemptResult(true);
  writer.RecordDumpWithoutCrashAttemptResult(false);
  writer.RecordDumpWithoutCrashAttemptResult(false);

  values = CrashReportingMetrics(registry_path()).RetrieveAndResetMetrics();
  EXPECT_EQ(2, values.crash_dump_attempts);
  EXPECT_EQ(2, values.successful_crash_dumps);
  EXPECT_EQ(0, values.failed_crash_dumps);
  EXPECT_EQ(3, values.dump_without_crash_attempts);
  EXPECT_EQ(1, values.successful_dumps_without_crash);
  EXPECT_EQ(2, values.failed_dumps_without_crash);
}

} // namespace browser_watcher
