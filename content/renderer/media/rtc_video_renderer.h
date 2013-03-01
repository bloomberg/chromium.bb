// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_LOCAL_VIDEO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_LOCAL_VIDEO_RENDERER_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "ui/gfx/size.h"
#include "webkit/media/video_frame_provider.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// RTCVideoRenderer is a webkit_media::VideoFrameProvider designed for rendering
// Video MediaStreamTracks,
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html#mediastreamtrack
// RTCVideoRenderer implements webrtc::VideoRendererInterface in order to render
// video frames provided from a webrtc::VideoTrackInteface.
// RTCVideoRenderer register itself to the Video Track when the
// VideoFrameProvider is started and deregisters itself when it is stopped.
// Calls to webrtc::VideoTrackInterface must occur on the main thread.
// TODO(wuchengli): Add unit test. See the link below for reference.
// http://src.chromium.org/viewvc/chrome/trunk/src/content/renderer/media/rtc_vi
// deo_decoder_unittest.cc?revision=180591&view=markup
class CONTENT_EXPORT RTCVideoRenderer
    : NON_EXPORTED_BASE(public webkit_media::VideoFrameProvider),
      NON_EXPORTED_BASE(public webrtc::VideoRendererInterface),
      NON_EXPORTED_BASE(public webrtc::ObserverInterface) {
 public:
  RTCVideoRenderer(
      webrtc::VideoTrackInterface* video_track,
      const base::Closure& error_cb,
      const RepaintCB& repaint_cb);

  // webkit_media::VideoFrameProvider implementation. Called on the main thread.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;

  // webrtc::VideoRendererInterface implementation. May be called on
  // a different thread.
  virtual void SetSize(int width, int height) OVERRIDE;
  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

 protected:
  virtual ~RTCVideoRenderer();

 private:
  enum State {
    kStarted,
    kPaused,
    kStopped,
  };

  void MaybeRenderSignalingFrame();
  void DoRenderFrameOnMainThread(scoped_refptr<media::VideoFrame> video_frame);

  base::Closure error_cb_;
  RepaintCB repaint_cb_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  State state_;

  // The video track the renderer is connected to.
  scoped_refptr<webrtc::VideoTrackInterface> video_track_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_LOCAL_VIDEO_RENDERER_H_
