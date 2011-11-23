// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/filters/video_renderer_base.h"
#include "ui/gfx/gl/gl_bindings.h"

class MessageLoop;

class GlVideoRenderer : public media::VideoRendererBase {
 public:
  GlVideoRenderer(Display* display, Window window,
                  MessageLoop* main_message_loop);

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder) OVERRIDE;
  virtual void OnStop(const base::Closure& callback) OVERRIDE;
  virtual void OnFrameAvailable() OVERRIDE;

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<GlVideoRenderer>;
  virtual ~GlVideoRenderer();

  void PaintOnMainThread();

  Display* display_;
  Window window_;

  // GL context.
  GLXContext gl_context_;

  // 3 textures, one for each plane.
  GLuint textures_[3];

  MessageLoop* main_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(GlVideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
