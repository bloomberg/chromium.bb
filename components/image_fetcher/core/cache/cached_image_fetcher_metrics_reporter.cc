// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"

namespace image_fetcher {

const char CachedImageFetcherMetricsReporter::
    kCachedImageFetcherInternalUmaClientName[] = "Internal";

namespace {

// 10 seconds in milliseconds.
const int kMaxReportTimeMs = 10 * 1000;

constexpr char kEventsHistogram[] = "CachedImageFetcher.Events";
constexpr char kImageLoadFromCacheHistogram[] =
    "CachedImageFetcher.ImageLoadFromCacheTime";
constexpr char kImageLoadFromCacheJavaHistogram[] =
    "CachedImageFetcher.ImageLoadFromCacheTimeJava";
constexpr char kImageLoadFromNetworkHistogram[] =
    "CachedImageFetcher.ImageLoadFromNetworkTime";
constexpr char kImageLoadFromNetworkAfterCacheHitHistogram[] =
    "CachedImageFetcher.ImageLoadFromNetworkAfterCacheHit";

// Returns a raw pointer to a histogram which is owned
base::HistogramBase* GetTimeHistogram(const std::string& histogram_name,
                                      const std::string client_name) {
  return base::LinearHistogram::FactoryTimeGet(
      histogram_name + std::string(".") + client_name, base::TimeDelta(),
      base::TimeDelta::FromMilliseconds(kMaxReportTimeMs),
      /* one bucket every 20ms. */ kMaxReportTimeMs / 20,
      base::Histogram::kUmaTargetedHistogramFlag);
}

}  // namespace

// static
void CachedImageFetcherMetricsReporter::ReportEvent(
    const std::string& client_name,
    CachedImageFetcherEvent event) {
  DCHECK(!client_name.empty());
  UMA_HISTOGRAM_ENUMERATION(kEventsHistogram, event);
  base::LinearHistogram::FactoryGet(
      kEventsHistogram + std::string(".") + client_name, 0,
      static_cast<int>(CachedImageFetcherEvent::kMaxValue),
      static_cast<int>(CachedImageFetcherEvent::kMaxValue),
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(event));
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromCacheHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromCacheHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromCacheJavaHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromCacheJavaHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromNetworkHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromNetworkHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromNetworkAfterCacheHitHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromNetworkAfterCacheHitHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void CachedImageFetcherMetricsReporter::ReportTimeSinceLastCacheLRUEviction(
    base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES("CachedImageFetcher.TimeSinceLastCacheLRUEviction",
                      time_delta);
}

}  // namespace image_fetcher
