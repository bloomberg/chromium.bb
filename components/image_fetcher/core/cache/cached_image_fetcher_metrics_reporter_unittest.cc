// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_fetcher {

namespace {

const char kUmaClientName[] = "foo";
const char kUmaClientNameOther[] = "bar";

const char kCachedImageFetcherEventHistogramName[] =
    "CachedImageFetcher.Events";
const char kCacheLoadHistogramName[] =
    "CachedImageFetcher.ImageLoadFromCacheTime";
const char kCacheLoadHistogramNameJava[] =
    "CachedImageFetcher.ImageLoadFromCacheTimeJava";
const char kNetworkLoadHistogramName[] =
    "CachedImageFetcher.ImageLoadFromNetworkTime";
const char kNetworkLoadAfterCacheHitHistogram[] =
    "CachedImageFetcher.ImageLoadFromNetworkAfterCacheHit";
const char kTimeSinceLastCacheLRUEviction[] =
    "CachedImageFetcher.TimeSinceLastCacheLRUEviction";

}  // namespace

class CachedImageFetcherMetricsReporterTest : public testing::Test {
 public:
  CachedImageFetcherMetricsReporterTest() {}
  ~CachedImageFetcherMetricsReporterTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherMetricsReporterTest);
};

TEST_F(CachedImageFetcherMetricsReporterTest, TestReportEvent) {
  CachedImageFetcherMetricsReporter::ReportEvent(
      kUmaClientName, CachedImageFetcherEvent::kCacheHit);
  CachedImageFetcherMetricsReporter::ReportEvent(
      kUmaClientNameOther, CachedImageFetcherEvent::kCacheHit);
  histogram_tester().ExpectBucketCount(kCachedImageFetcherEventHistogramName,
                                       CachedImageFetcherEvent::kCacheHit, 2);
  histogram_tester().ExpectBucketCount(
      std::string(kCachedImageFetcherEventHistogramName)
          .append(".")
          .append(kUmaClientName),
      CachedImageFetcherEvent::kCacheHit, 1);
  histogram_tester().ExpectBucketCount(
      std::string(kCachedImageFetcherEventHistogramName)
          .append(".")
          .append(kUmaClientNameOther),
      CachedImageFetcherEvent::kCacheHit, 1);
}

TEST_F(CachedImageFetcherMetricsReporterTest,
       TestReportImageLoadFromCacheTime) {
  CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
      kUmaClientName, base::Time());
  CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kCacheLoadHistogramName, 2);
  histogram_tester().ExpectTotalCount(
      std::string(kCacheLoadHistogramName).append(".").append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(std::string(kCacheLoadHistogramName)
                                          .append(".")
                                          .append(kUmaClientNameOther),
                                      1);
}

TEST_F(CachedImageFetcherMetricsReporterTest,
       TestReportImageLoadFromCacheTimeJava) {
  CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(
      kUmaClientName, base::Time());
  CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kCacheLoadHistogramNameJava, 2);
  histogram_tester().ExpectTotalCount(std::string(kCacheLoadHistogramNameJava)
                                          .append(".")
                                          .append(kUmaClientName),
                                      1);
  histogram_tester().ExpectTotalCount(std::string(kCacheLoadHistogramNameJava)
                                          .append(".")
                                          .append(kUmaClientNameOther),
                                      1);
}

TEST_F(CachedImageFetcherMetricsReporterTest,
       TestReportImageLoadFromNetworkTime) {
  CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
      kUmaClientName, base::Time());
  CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kNetworkLoadHistogramName, 2);
  histogram_tester().ExpectTotalCount(
      std::string(kNetworkLoadHistogramName).append(".").append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(std::string(kNetworkLoadHistogramName)
                                          .append(".")
                                          .append(kUmaClientNameOther),
                                      1);
}

TEST_F(CachedImageFetcherMetricsReporterTest,
       TestReportImageLoadFromNetworkAfterCacheHit) {
  CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
      kUmaClientName, base::Time());
  CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kNetworkLoadAfterCacheHitHistogram, 2);
  histogram_tester().ExpectTotalCount(
      std::string(kNetworkLoadAfterCacheHitHistogram)
          .append(".")
          .append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(
      std::string(kNetworkLoadAfterCacheHitHistogram)
          .append(".")
          .append(kUmaClientNameOther),
      1);
}

TEST_F(CachedImageFetcherMetricsReporterTest,
       TestReportTimeSinceLastCacheLRUEviction) {
  CachedImageFetcherMetricsReporter::ReportTimeSinceLastCacheLRUEviction(
      base::Time());
  histogram_tester().ExpectTotalCount(kTimeSinceLastCacheLRUEviction, 1);
}

}  // namespace image_fetcher
