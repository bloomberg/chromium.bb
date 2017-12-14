// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_metrics_provider.h"

#include "media/mojo/services/video_decode_stats_recorder.h"
#include "media/mojo/services/watch_time_recorder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {

static uint64_t g_playback_id = 0;

MediaMetricsProvider::MediaMetricsProvider(VideoDecodePerfHistory* perf_history)
    : playback_id_(g_playback_id++), perf_history_(perf_history) {}

MediaMetricsProvider::~MediaMetricsProvider() {
  // TODO(dalecurtis): Log final playback metrics using |playback_id_| here.
}

void MediaMetricsProvider::AcquireWatchTimeRecorder(
    mojom::PlaybackPropertiesPtr properties,
    mojom::WatchTimeRecorderRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<WatchTimeRecorder>(std::move(properties), playback_id_),
      std::move(request));
}

void MediaMetricsProvider::AcquireVideoDecodeStatsRecorder(
    const url::Origin& untrusted_top_frame_origin,
    bool is_top_frame,
    mojom::VideoDecodeStatsRecorderRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<VideoDecodeStatsRecorder>(
                              untrusted_top_frame_origin, is_top_frame,
                              playback_id_, perf_history_),
                          std::move(request));
}

// static
void MediaMetricsProvider::Create(VideoDecodePerfHistory* perf_history,
                                  mojom::MediaMetricsProviderRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<MediaMetricsProvider>(perf_history),
                          std::move(request));
}

}  // namespace media
