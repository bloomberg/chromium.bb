// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace content {

// VideoTrackRecorder is a MediaStreamVideoSink that encodes the video frames
// received from a Stream Video Track. The class is constructed and used on a
// single thread, namely the main Render thread. It has an internal VpxEncoder
// that uses a worker thread for encoding.
class CONTENT_EXPORT VideoTrackRecorder
    : NON_EXPORTED_BASE(public MediaStreamVideoSink) {
 public:
  using OnEncodedVideoCB =
      base::Callback<void(const scoped_refptr<media::VideoFrame>& video_frame,
                          const base::StringPiece& encoded_data,
                          base::TimeTicks capture_timestamp,
                          bool is_key_frame)>;

  VideoTrackRecorder(const blink::WebMediaStreamTrack& track,
                     const OnEncodedVideoCB& on_encoded_video_cb);
  ~VideoTrackRecorder() override;

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const base::TimeTicks& capture_time);

 private:
  friend class VideoTrackRecorderTest;

  // Used to check that we are destroyed on the same thread we were created.
  base::ThreadChecker main_render_thread_checker_;

  // We need to hold on to the Blink track to remove ourselves on dtor.
  blink::WebMediaStreamTrack track_;

  // Forward declaration and member of an inner class to encode using VPx.
  class VpxEncoder;
  const scoped_ptr<VpxEncoder> encoder_;

  base::WeakPtrFactory<VideoTrackRecorder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoTrackRecorder);
};

}  // namespace content
#endif  // CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_
