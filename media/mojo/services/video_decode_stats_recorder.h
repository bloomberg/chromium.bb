// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_RECORDER_H_
#define MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_RECORDER_H_

#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "media/mojo/interfaces/video_decode_stats_recorder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "url/gurl.h"

namespace media {

class VideoDecodePerfHistory;

// See mojom::VideoDecodeStatsRecorder for documentation.
class MEDIA_MOJO_EXPORT VideoDecodeStatsRecorder
    : public mojom::VideoDecodeStatsRecorder {
 public:
  // See Create().
  explicit VideoDecodeStatsRecorder(VideoDecodePerfHistory* perf_history);

  ~VideoDecodeStatsRecorder() override;

  // |perf_history| required to save decode stats to local database and report
  // metrics. Callers must ensure that |perf_history| outlives this object.
  static void Create(VideoDecodePerfHistory* perf_history,
                     mojom::VideoDecodeStatsRecorderRequest request);

  // mojom::VideoDecodeStatsRecorder implementation:
  void SetPageInfo(const url::Origin& untrusted_top_frame_origin,
                   bool is_top_frame) override;
  void StartNewRecord(VideoCodecProfile profile,
                      const gfx::Size& natural_size,
                      int frames_per_sec) override;
  void UpdateRecord(uint32_t frames_decoded,
                    uint32_t frames_dropped,
                    uint32_t frames_decoded_power_efficient) override;

 private:
  // Save most recent stats values to disk. Called during destruction and upon
  // starting a new record.
  void FinalizeRecord();

  url::Origin untrusted_top_frame_origin_;
  bool is_top_frame_;
  VideoDecodePerfHistory* perf_history_;
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  gfx::Size natural_size_;
  int frames_per_sec_ = 0;
  uint32_t frames_decoded_ = 0;
  uint32_t frames_dropped_ = 0;
  uint32_t frames_decoded_power_efficient_ = 0;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsRecorder);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_RECORDER_H_
