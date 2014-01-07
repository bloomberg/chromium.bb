// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_PROXY_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_PROXY_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/video/capture/video_capture.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// This is a helper class to proxy a VideoCapture::EventHandler. In the renderer
// process, the VideoCaptureImpl calls its handler on a "Video Capture" thread,
// this class allows seamless proxying to another thread ("main thread"), which
// would be the thread where the instance of this class is created. The
// "proxied" handler is then called on that thread.
// Since the VideoCapture is living on the "Video Capture" thread, querying its
// state from the "main thread" is fundamentally racy. Instead this class keeps
// track of the state every time it is called by the VideoCapture (on the VC
// thread), and forwards that information to the main thread.
class MEDIA_EXPORT VideoCaptureHandlerProxy
    : public VideoCapture::EventHandler {
 public:
  struct VideoCaptureState {
    VideoCaptureState() : started(false), frame_rate(0) {}
    bool started;
    int frame_rate;
  };

  // Called on main thread.
  VideoCaptureHandlerProxy(
      VideoCapture::EventHandler* proxied,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  virtual ~VideoCaptureHandlerProxy();

  // Retrieves the state of the VideoCapture. Must be called on main thread.
  const VideoCaptureState& state() const { return state_; }

  // VideoCapture::EventHandler implementation, called on VC thread.
  virtual void OnStarted(VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(VideoCapture* capture) OVERRIDE;
  virtual void OnError(VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(VideoCapture* capture) OVERRIDE;
  virtual void OnFrameReady(VideoCapture* capture,
                            const scoped_refptr<VideoFrame>& frame) OVERRIDE;

 private:
  // Called on main thread.
  void OnStartedOnMainThread(
      VideoCapture* capture,
      const VideoCaptureState& state);
  void OnStoppedOnMainThread(
      VideoCapture* capture,
      const VideoCaptureState& state);
  void OnPausedOnMainThread(
      VideoCapture* capture,
      const VideoCaptureState& state);
  void OnErrorOnMainThread(
      VideoCapture* capture,
      const VideoCaptureState& state,
      int error_code);
  void OnRemovedOnMainThread(
      VideoCapture* capture,
      const VideoCaptureState& state);
  void OnFrameReadyOnMainThread(VideoCapture* capture,
                                const VideoCaptureState& state,
                                const scoped_refptr<VideoFrame>& frame);

  // Only accessed from main thread.
  VideoCapture::EventHandler* proxied_;
  VideoCaptureState state_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_PROXY_H_
