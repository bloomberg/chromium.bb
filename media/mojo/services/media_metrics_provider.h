// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_

#include "media/mojo/interfaces/media_metrics_provider.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "url/origin.h"

namespace media {
class VideoDecodePerfHistory;

// See mojom::MediaMetricsProvider for documentation.
class MEDIA_MOJO_EXPORT MediaMetricsProvider
    : public mojom::MediaMetricsProvider {
 public:
  explicit MediaMetricsProvider(VideoDecodePerfHistory* perf_history);
  ~MediaMetricsProvider() override;

  // Creates a MediaMetricsProvider, |perf_history| may be nullptr if perf
  // history database recording is disabled.
  static void Create(VideoDecodePerfHistory* perf_history,
                     mojom::MediaMetricsProviderRequest request);

 private:
  // mojom::MediaMetricsProvider implementation:
  void AcquireWatchTimeRecorder(
      mojom::PlaybackPropertiesPtr properties,
      mojom::WatchTimeRecorderRequest request) override;
  void AcquireVideoDecodeStatsRecorder(
      const url::Origin& untrusted_top_frame_origin,
      bool is_top_frame,
      mojom::VideoDecodeStatsRecorderRequest request) override;

  // Session unique ID which maps to a given WebMediaPlayerImpl instances. Used
  // to coordinate multiply logged events with a singly logged metric.
  const uint64_t playback_id_;

  VideoDecodePerfHistory* const perf_history_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetricsProvider);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_
