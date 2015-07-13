// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/request_source_bandwidth_histograms.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/process_type.h"
#include "net/url_request/url_request.h"

namespace {

enum Bucket {
  BUCKET_UNKNOWN,
  BUCKET_RENDERER,
  BUCKET_BROWSER,
  BUCKET_MAX,
};

bool ShouldHistogramRequest(const net::URLRequest* request, bool started) {
  return started &&
         !request->was_cached() &&
         request->url().SchemeIsHTTPOrHTTPS();
}

Bucket GetBucketForRequest(const net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return BUCKET_BROWSER;
  else if (info->GetProcessType() == content::PROCESS_TYPE_RENDERER)
    return BUCKET_RENDERER;
  else
    return BUCKET_UNKNOWN;
}

// Histogram response sizes in kilobytes, from 1 KB to 4 GB.
#define UMA_HISTOGRAM_RESPONSE_KB(bucket, sample) \
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ResponseSizeByProcess." bucket, sample, \
                                1, 4 * 1024 * 1024, 100)

void LogRequest(Bucket bucket, int64 bytes) {
  int64 kilobytes = bytes / 1024;
  switch (bucket) {
    case BUCKET_UNKNOWN:
      UMA_HISTOGRAM_RESPONSE_KB("Unknown", kilobytes);
      break;
    case BUCKET_RENDERER:
      UMA_HISTOGRAM_RESPONSE_KB("Renderer", kilobytes);
      break;
    case BUCKET_BROWSER:
      UMA_HISTOGRAM_RESPONSE_KB("Browser", kilobytes);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

void RecordRequestSourceBandwidth(const net::URLRequest* request,
                                  bool started) {
  if (ShouldHistogramRequest(request, started))
    LogRequest(GetBucketForRequest(request), request->GetTotalReceivedBytes());
}
