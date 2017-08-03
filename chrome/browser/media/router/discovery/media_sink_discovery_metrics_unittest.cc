// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;

namespace media_router {

TEST(DialDeviceCountMetricsTest, RecordDeviceCountsIfNeeded) {
  DialDeviceCountMetrics metrics;
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  metrics.SetClockForTest(base::WrapUnique(clock));
  base::HistogramTester tester;
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 0);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 0);

  // Only record one count within one hour.
  clock->SetNow(base::Time::Now());
  metrics.RecordDeviceCountsIfNeeded(6, 10);
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 1);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 10, 1);

  // Record another count.
  clock->Advance(base::TimeDelta::FromHours(2));
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 2);
  tester.ExpectTotalCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 2);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount, 7, 1);
  tester.ExpectBucketCount(
      DialDeviceCountMetrics::kHistogramDialKnownDeviceCount, 10, 2);
}

TEST(CastDeviceCountMetricsTest, RecordDeviceCountsIfNeeded) {
  CastDeviceCountMetrics metrics;
  base::SimpleTestClock* clock = new base::SimpleTestClock();
  metrics.SetClockForTest(base::WrapUnique(clock));
  base::HistogramTester tester;
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 0);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 0);

  // Only record one count within one hour.
  clock->SetNow(base::Time::Now());
  metrics.RecordDeviceCountsIfNeeded(6, 10);
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 1);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 10, 1);

  // Record another count.
  clock->Advance(base::TimeDelta::FromHours(2));
  metrics.RecordDeviceCountsIfNeeded(7, 10);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 2);
  tester.ExpectTotalCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 2);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 6, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount, 7, 1);
  tester.ExpectBucketCount(
      CastDeviceCountMetrics::kHistogramCastKnownDeviceCount, 10, 2);
}

}  // namespace media_router
