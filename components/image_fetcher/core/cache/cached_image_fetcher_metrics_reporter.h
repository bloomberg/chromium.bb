// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_CACHED_IMAGE_FETCHER_METRICS_REPORTER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_CACHED_IMAGE_FETCHER_METRICS_REPORTER_H_

#include "base/time/time.h"

namespace image_fetcher {

// Enum for the result of the fetch, reported through UMA. Present in enums.xml
// as CachedImageFetcherEvent. New values should be added at the end and things
// should not be renumbered.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.cached_image_fetcher)
enum class CachedImageFetcherEvent {
  kImageRequest = 0,
  kCacheHit = 1,
  kCacheMiss = 2,
  kCacheDecodingError = 3,
  kTranscodingError = 4,
  kTotalFailure = 5,
  kCacheStartupEvictionStarted = 6,
  kCacheStartupEvictionFinished = 7,
  kJavaInMemoryCacheHit = 8,
  kJavaDiskCacheHit = 9,
  kMaxValue = kJavaDiskCacheHit,
};

class CachedImageFetcherMetricsReporter {
 public:
  // Report cache events, used by CachedImageFetcher and composing classes.
  static void ReportEvent(CachedImageFetcherEvent event);

  // Report the time it takes to load an image from the cache in native code.
  static void ReportImageLoadFromCacheTime(base::Time start_time);

  // Report the time it takes to load an image from the network.
  static void ReportImageLoadFromNetworkTime(base::Time start_time);

  // Report the time it takes to load an image from the network after a cache
  // hit.
  static void ReportImageLoadFromNetworkAfterCacheHit(base::Time start_time);

  // Report the time between cache evictions.
  static void ReportTimeSinceLastCacheLRUEviction(base::Time start_time);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_CACHED_IMAGE_FETCHER_METRICS_REPORTER_H_