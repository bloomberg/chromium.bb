// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_

#include "app/gfx/gl/gl_bindings.h"
#include "base/scoped_ptr.h"
#include "media/base/filters.h"
#include "media/filters/video_renderer_base.h"

class GlVideoRenderer : public media::VideoRendererBase {
 public:
  GlVideoRenderer(Display* display, Window window, MessageLoop* message_loop);

  // This method is called to paint the current video frame to the assigned
  // window.
  void Paint();

  static GlVideoRenderer* instance() { return instance_; }

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
  friend class scoped_refptr<GlVideoRenderer>;
  virtual ~GlVideoRenderer();

  Display* display_;
  Window window_;

  // GL context.
  GLXContext gl_context_;

  // 3 textures, one for each plane.
  GLuint textures_[3];

  MessageLoop* glx_thread_message_loop_;
  static GlVideoRenderer* instance_;

  DISALLOW_COPY_AND_ASSIGN(GlVideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
