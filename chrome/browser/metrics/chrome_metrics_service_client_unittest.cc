// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include "base/files/file_path.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/process_handle.h"
#include "components/metrics/file_metrics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool TestIsProcessRunning(base::ProcessId pid) {
  // Odd are running, even are not.
  return (pid & 1) == 1;
}

TEST(ChromeMetricsServiceClientTest, FilterFiles) {
  ChromeMetricsServiceClient::SetIsProcessRunningForTesting(
      &TestIsProcessRunning);

  base::ProcessId my_pid = base::GetCurrentProcId();
  base::FilePath active_dir(FILE_PATH_LITERAL("foo"));
  base::FilePath upload_dir(FILE_PATH_LITERAL("bar"));
  base::FilePath upload_path;
  base::GlobalHistogramAllocator::ConstructFilePathsForUploadDir(
      active_dir, upload_dir, "TestMetrics", &upload_path, nullptr, nullptr);
  EXPECT_EQ(metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID,
            ChromeMetricsServiceClient::FilterBrowserMetricsFiles(upload_path));

  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_PROCESS_FILE,
      ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 10)));
  EXPECT_EQ(
      metrics::FileMetricsProvider::FILTER_TRY_LATER,
      ChromeMetricsServiceClient::FilterBrowserMetricsFiles(
          base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
              upload_dir, "Test", base::Time::Now(), (my_pid & ~1) + 11)));
}

}  // namespace
