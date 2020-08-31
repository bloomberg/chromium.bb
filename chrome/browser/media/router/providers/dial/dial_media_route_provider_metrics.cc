// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace media_router {

// static
void DialMediaRouteProviderMetrics::RecordCreateRouteResult(
    DialCreateRouteResult result) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramDialCreateRouteResult, result,
                            DialCreateRouteResult::kCount);
}

// static
void DialMediaRouteProviderMetrics::RecordTerminateRouteResult(
    DialTerminateRouteResult result) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramDialTerminateRouteResult, result,
                            DialTerminateRouteResult::kCount);
}

// static
void DialMediaRouteProviderMetrics::RecordParseMessageResult(
    DialParseMessageResult result) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramDialParseMessageResult, result,
                            DialParseMessageResult::kCount);
}

}  // namespace media_router
