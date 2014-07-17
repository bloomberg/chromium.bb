// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/test/base/uma_histogram_helper.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"

namespace chromeos {  // Need this because of the FRIEND_TEST

class ExternalMetricsTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    external_metrics_ = ExternalMetrics::CreateForTesting(
        dir_.path().Append("testfile").value());

    base::StatisticsRecorder::Initialize();
  }

  base::ScopedTempDir dir_;
  scoped_refptr<ExternalMetrics> external_metrics_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ExternalMetricsTest, HandleMissingFile) {
  ASSERT_TRUE(base::DeleteFile(
      base::FilePath(external_metrics_->uma_events_file_), false));

  EXPECT_EQ(0, external_metrics_->CollectEvents());
}

TEST_F(ExternalMetricsTest, CanReceiveHistogram) {
  scoped_ptr<metrics::MetricSample> hist =
      metrics::MetricSample::HistogramSample("foo", 2, 1, 100, 10);

  EXPECT_TRUE(metrics::SerializationUtils::WriteMetricToFile(
      *hist.get(), external_metrics_->uma_events_file_));

  EXPECT_EQ(1, external_metrics_->CollectEvents());

  UMAHistogramHelper helper;
  helper.Fetch();
  helper.ExpectTotalCount("foo", 1);
}

TEST_F(ExternalMetricsTest, IncorrectHistogramsAreDiscarded) {
  // Malformed histogram (min > max).
  scoped_ptr<metrics::MetricSample> hist =
      metrics::MetricSample::HistogramSample("bar", 30, 200, 20, 10);

  EXPECT_TRUE(metrics::SerializationUtils::WriteMetricToFile(
      *hist.get(), external_metrics_->uma_events_file_));

  external_metrics_->CollectEvents();

  UMAHistogramHelper helper;
  helper.Fetch();
  helper.ExpectTotalCount("bar", 0);
}

}  // namespace chromeos
