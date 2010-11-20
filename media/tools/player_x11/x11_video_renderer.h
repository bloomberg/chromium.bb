// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_

#include <X11/Xlib.h>

#include "base/scoped_ptr.h"
#include "media/base/filters.h"
#include "media/filters/video_renderer_base.h"

class X11VideoRenderer : public media::VideoRendererBase {
 public:
  X11VideoRenderer(Display* display, Window window, MessageLoop* message_loop);

  // This method is called to paint the current video frame to the assigned
  // window.
  void Paint();

  static X11VideoRenderer* instance() { return instance_; }

  MessageLoop* glx_thread_message_loop() {
    return glx_thread_message_loop_;
  }

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder);
  virtual void OnStop(media::FilterCallback* callback);
  virtual void OnFrameAvailable();

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<X11VideoRenderer>;
  virtual ~X11VideoRenderer();

  Display* display_;
  Window window_;

  // Image in heap that contains the RGBA data of the video frame.
  XImage* image_;

  // Picture represents the paint target. This is a picture located
  // in the server.
  unsigned long picture_;

  bool use_render_;

  MessageLoop* glx_thread_message_loop_;
  static X11VideoRenderer* instance_;

  DISALLOW_COPY_AND_ASSIGN(X11VideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
