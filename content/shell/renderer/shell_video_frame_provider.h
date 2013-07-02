// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_SHELL_VIDEO_FRAME_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_SHELL_VIDEO_FRAME_PROVIDER_H_

#include "base/time/time.h"
#include "ui/gfx/size.h"
#include "webkit/renderer/media/video_frame_provider.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// A simple implementation of VideoFrameProvider generates raw frames and
// passes them to webmediaplayer.
// Since non-black pixel values are required in the layout test, e.g.,
// media/video-capture-canvas.html, this class should generate frame with
// only non-black pixels.
class ShellVideoFrameProvider : public webkit_media::VideoFrameProvider {
 public:
  ShellVideoFrameProvider(
      const gfx::Size& size,
      const base::TimeDelta& frame_duration,
      const base::Closure& error_cb,
      const RepaintCB& repaint_cb);

  // webkit_media::VideoFrameProvider implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;

 protected:
  virtual ~ShellVideoFrameProvider();

 private:
  enum State {
    kStarted,
    kPaused,
    kStopped,
  };

  void GenerateFrame();

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  gfx::Size size_;
  State state_;

  base::TimeDelta current_time_;
  base::TimeDelta frame_duration_;
  base::Closure error_cb_;
  RepaintCB repaint_cb_;

  DISALLOW_COPY_AND_ASSIGN(ShellVideoFrameProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_SHELL_VIDEO_FRAME_PROVIDER_H_
