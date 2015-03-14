// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/video_frame_provider.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// RTCVideoRenderer is a VideoFrameProvider designed for rendering Video
// MediaStreamTracks [1], RTCVideoRenderer implements MediaStreamVideoSink in
// order to render video frames provided from a MediaStreamVideoTrack, to which
// it AddToVideoTrack()s itself when the VideoFrameProvider is Start()ed
// and RemoveFromVideoTrack()s itself when the latter is Stop()ed.
//
// [1] http://dev.w3.org/2011/webrtc/editor/getusermedia.html#mediastreamtrack
//
// TODO(wuchengli): Add unit test. See the link below for reference.
// http://src.chromium.org/viewvc/chrome/trunk/src/content/renderer/media/rtc_vi
// deo_decoder_unittest.cc?revision=180591&view=markup
class CONTENT_EXPORT RTCVideoRenderer
    : NON_EXPORTED_BASE(public VideoFrameProvider),
      NON_EXPORTED_BASE(public MediaStreamVideoSink) {
 public:
  RTCVideoRenderer(const blink::WebMediaStreamTrack& video_track,
                   const base::Closure& error_cb,
                   const RepaintCB& repaint_cb);

  // VideoFrameProvider implementation. Called on the main thread.
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;

 protected:
  ~RTCVideoRenderer() override;

 private:
  enum State {
    STARTED,
    PAUSED,
    STOPPED,
  };

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    const base::TimeTicks& estimated_capture_time);

  // MediaStreamVideoSink implementation. Called on the main thread.
  void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) override;

  void RenderSignalingFrame();

  const base::Closure error_cb_;
  const RepaintCB repaint_cb_;
  const scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  State state_;
  gfx::Size frame_size_;
  const blink::WebMediaStreamTrack video_track_;
  base::WeakPtrFactory<RTCVideoRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_RENDERER_H_
