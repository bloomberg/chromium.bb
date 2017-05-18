// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/media_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"

namespace {

const char kHistogramNetworkBytes[] =
    "PageLoad.Clients.MediaPageLoad.Experimental.Bytes.Network";
const char kHistogramCacheBytes[] =
    "PageLoad.Clients.MediaPageLoad.Experimental.Bytes.Cache";
const char kHistogramTotalBytes[] =
    "PageLoad.Clients.MediaPageLoad.Experimental.Bytes.Total";

}  // namespace

MediaPageLoadMetricsObserver::MediaPageLoadMetricsObserver()
    : cache_bytes_(0), network_bytes_(0), played_media_(false) {}

MediaPageLoadMetricsObserver::~MediaPageLoadMetricsObserver() = default;

void MediaPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.was_cached) {
    cache_bytes_ += extra_request_complete_info.raw_body_bytes;
  } else {
    network_bytes_ += extra_request_complete_info.raw_body_bytes;
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MediaPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (info.did_commit && played_media_) {
    RecordByteHistograms();
  }
  return STOP_OBSERVING;
}

void MediaPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!played_media_)
    return;
  RecordByteHistograms();
}

void MediaPageLoadMetricsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    bool is_in_main_frame) {
  if (played_media_)
    return;
  // Track media (audio or video) in all frames of the page load.
  played_media_ = true;
}

void MediaPageLoadMetricsObserver::RecordByteHistograms() {
  DCHECK(played_media_);
  PAGE_BYTES_HISTOGRAM(kHistogramNetworkBytes, network_bytes_);
  PAGE_BYTES_HISTOGRAM(kHistogramCacheBytes, cache_bytes_);
  PAGE_BYTES_HISTOGRAM(kHistogramTotalBytes, network_bytes_ + cache_bytes_);
}
