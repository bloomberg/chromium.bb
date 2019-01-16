// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/video_frame_pool.h"
#include "third_party/webrtc/media/base/adapted_video_track_source.h"
#include "third_party/webrtc/rtc_base/timestamp_aligner.h"

namespace content {

// This class implements webrtc's VideoTrackSourceInterface. To pass frames down
// the webrtc video pipeline, each received a media::VideoFrame is converted to
// a webrtc::VideoFrame, taking any adaptation requested by downstream classes
// into account.
class CONTENT_EXPORT WebRtcVideoTrackSource
    : public rtc::AdaptedVideoTrackSource {
 public:
  WebRtcVideoTrackSource(bool is_screencast,
                         absl::optional<bool> needs_denoising);
  ~WebRtcVideoTrackSource() override;

  SourceState state() const override;

  bool remote() const override;
  bool is_screencast() const override;
  absl::optional<bool> needs_denoising() const override;
  void OnFrameCaptured(const scoped_refptr<media::VideoFrame>& frame);

  using webrtc::VideoTrackSourceInterface::AddOrUpdateSink;
  using webrtc::VideoTrackSourceInterface::RemoveSink;

 private:
  // |thread_checker_| is bound to the libjingle worker thread.
  THREAD_CHECKER(thread_checker_);
  media::VideoFramePool scaled_frame_pool_;
  // State for the timestamp translation.
  rtc::TimestampAligner timestamp_aligner_;

  const bool is_screencast_;
  const absl::optional<bool> needs_denoising_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoTrackSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_TRACK_SOURCE_H_
