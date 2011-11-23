// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_

#include <X11/Xlib.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/filters/video_renderer_base.h"

class MessageLoop;

class X11VideoRenderer : public media::VideoRendererBase {
 public:
  X11VideoRenderer(Display* display, Window window,
                   MessageLoop* main_message_loop);

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder) OVERRIDE;
  virtual void OnStop(const base::Closure& callback) OVERRIDE;
  virtual void OnFrameAvailable() OVERRIDE;

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<X11VideoRenderer>;
  virtual ~X11VideoRenderer();

  void PaintOnMainThread();

  Display* display_;
  Window window_;

  // Image in heap that contains the RGBA data of the video frame.
  XImage* image_;

  // Picture represents the paint target. This is a picture located
  // in the server.
  unsigned long picture_;

  bool use_render_;

  MessageLoop* main_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(X11VideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
