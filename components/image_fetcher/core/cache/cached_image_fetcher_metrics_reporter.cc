// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"

#include "base/metrics/histogram_macros.h"

namespace image_fetcher {

// static
void CachedImageFetcherMetricsReporter::ReportEvent(
    CachedImageFetcherEvent event) {
  UMA_HISTOGRAM_ENUMERATION("CachedImageFetcher.Events", event);
}

// static
void CachedImageFetcherMetricsReporter::ReportLoadTime(LoadTimeType type,
                                                       base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  switch (type) {
    case LoadTimeType::kLoadFromCache:
      UMA_HISTOGRAM_TIMES("CachedImageFetcher.ImageLoadFromCacheTime",
                          time_delta);
      break;
    case LoadTimeType::kLoadFromNetwork:
      UMA_HISTOGRAM_TIMES("CachedImageFetcher.ImageLoadFromNetworkTime",
                          time_delta);
      break;
    case LoadTimeType::kLoadFromNetworkAfterCacheHit:
      UMA_HISTOGRAM_TIMES(
          "CachedImageFetcher.ImageLoadFromNetworkAfterCacheHit", time_delta);
      break;
  }
}

}  // namespace image_fetcher