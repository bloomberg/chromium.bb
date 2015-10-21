// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

// VideoTrackRecorder is a MediaStreamVideoSink that encodes the video frames
// received from a Stream Video Track. The class is constructed and used on a
// single thread, namely the main Render thread. It has an internal VpxEncoder
// with its own threading subtleties, see the implementation file. This mirrors
// the other MediaStreamVideo* classes that are constructed/configured on Main
// Render thread but that pass frames on Render IO thread.
class CONTENT_EXPORT VideoTrackRecorder
    : NON_EXPORTED_BASE(public MediaStreamVideoSink) {
 public:
  using OnEncodedVideoCB =
      base::Callback<void(const scoped_refptr<media::VideoFrame>& video_frame,
                          scoped_ptr<std::string> encoded_data,
                          base::TimeTicks capture_timestamp,
                          bool is_key_frame)>;

  // |use_vp9| forces using VP9, otherwise VP8 will be used by default.
  VideoTrackRecorder(bool use_vp9,
                     const blink::WebMediaStreamTrack& track,
                     const OnEncodedVideoCB& on_encoded_video_cb);
  ~VideoTrackRecorder() override;

  void Pause();
  void Resume();

  void OnVideoFrameForTesting(const scoped_refptr<media::VideoFrame>& frame,
                              base::TimeTicks capture_time);
 private:
  friend class VideoTrackRecorderTest;

  // Used to check that we are destroyed on the same thread we were created.
  base::ThreadChecker main_render_thread_checker_;

  // We need to hold on to the Blink track to remove ourselves on dtor.
  blink::WebMediaStreamTrack track_;

  // Forward declaration and member of an inner class to encode using VPx.
  class VpxEncoder;
  const scoped_refptr<VpxEncoder> encoder_;

  DISALLOW_COPY_AND_ASSIGN(VideoTrackRecorder);
};

}  // namespace content
#endif  // CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_
