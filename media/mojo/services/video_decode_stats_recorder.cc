// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_decode_stats_recorder.h"
#include "base/memory/ptr_util.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#include "base/logging.h"

namespace media {

VideoDecodeStatsRecorder::VideoDecodeStatsRecorder(
    VideoDecodePerfHistory* perf_history)
    : perf_history_(perf_history) {
  DCHECK(perf_history_);
}

VideoDecodeStatsRecorder::~VideoDecodeStatsRecorder() {
  DVLOG(2) << __func__ << " Finalize for IPC disconnect";
  FinalizeRecord();
}

// static
void VideoDecodeStatsRecorder::Create(
    VideoDecodePerfHistory* perf_history,
    mojom::VideoDecodeStatsRecorderRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<VideoDecodeStatsRecorder>(perf_history),
      std::move(request));
}

void VideoDecodeStatsRecorder::SetPageInfo(
    const url::Origin& untrusted_top_frame_origin,
    bool is_top_frame) {
  DVLOG(2) << __func__
           << " untrusted_top_frame_origin:" << untrusted_top_frame_origin
           << " is_top_frame:" << is_top_frame;

  untrusted_top_frame_origin_ = untrusted_top_frame_origin;
  is_top_frame_ = is_top_frame;
}

void VideoDecodeStatsRecorder::StartNewRecord(VideoCodecProfile profile,
                                              const gfx::Size& natural_size,
                                              int frames_per_sec) {
  DCHECK_NE(profile, VIDEO_CODEC_PROFILE_UNKNOWN);
  DCHECK_GT(frames_per_sec, 0);
  DCHECK(natural_size.width() > 0 && natural_size.height() > 0);

  FinalizeRecord();

  DVLOG(2) << __func__ << "profile: " << profile
           << " sz:" << natural_size.ToString() << " fps:" << frames_per_sec;

  profile_ = profile;
  natural_size_ = natural_size;
  frames_per_sec_ = frames_per_sec;
  frames_decoded_ = 0;
  frames_dropped_ = 0;
  frames_decoded_power_efficient_ = 0;
}

void VideoDecodeStatsRecorder::UpdateRecord(
    uint32_t frames_decoded,
    uint32_t frames_dropped,
    uint32_t frames_decoded_power_efficient) {
  DVLOG(3) << __func__ << " decoded:" << frames_decoded
           << " dropped:" << frames_dropped;

  // Dropped can never exceed decoded.
  DCHECK_LE(frames_dropped, frames_decoded);
  // Power efficient frames can never exceed decoded frames.
  DCHECK_LE(frames_decoded_power_efficient, frames_decoded);
  // Should never go backwards.
  DCHECK_GE(frames_decoded, frames_decoded_);
  DCHECK_GE(frames_dropped, frames_dropped_);
  DCHECK_GE(frames_decoded_power_efficient, frames_decoded_power_efficient_);

  frames_decoded_ = frames_decoded;
  frames_dropped_ = frames_dropped;
  frames_decoded_power_efficient_ = frames_decoded_power_efficient;
}

void VideoDecodeStatsRecorder::FinalizeRecord() {
  if (profile_ == VIDEO_CODEC_PROFILE_UNKNOWN || frames_decoded_ == 0)
    return;

  DVLOG(2) << __func__ << " profile: " << profile_
           << " size:" << natural_size_.ToString() << " fps:" << frames_per_sec_
           << " decoded:" << frames_decoded_ << " dropped:" << frames_dropped_
           << " power efficient decoded:" << frames_decoded_power_efficient_;

  perf_history_->SavePerfRecord(untrusted_top_frame_origin_, is_top_frame_,
                                profile_, natural_size_, frames_per_sec_,
                                frames_decoded_, frames_dropped_,
                                frames_decoded_power_efficient_);
}

}  // namespace media
