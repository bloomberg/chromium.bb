// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_

#include <GL/glew.h>
#include <GL/glxew.h>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "media/base/factory.h"
#include "media/filters/video_renderer_base.h"

class GlVideoRenderer : public media::VideoRendererBase {
 public:
  static media::FilterFactory* CreateFactory(Display* display,
                                             Window window) {
    return new media::FilterFactoryImpl2<
        GlVideoRenderer, Display*, Window>(display, window);
  }

  GlVideoRenderer(Display* display, Window window);

  // This method is called to paint the current video frame to the assigned
  // window.
  void Paint();

  // media::FilterFactoryImpl2 Implementation.
  static bool IsMediaFormatSupported(const media::MediaFormat& media_format);

  static GlVideoRenderer* instance() { return instance_; }

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder);
  virtual void OnStop();
  virtual void OnFrameAvailable();

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<GlVideoRenderer>;
  virtual ~GlVideoRenderer();

  int width_;
  int height_;

  Display* display_;
  Window window_;

  // Protects |new_frame_|.
  Lock lock_;
  bool new_frame_;

  // GL context.
  GLXContext gl_context_;

  // 3 textures, one for each plane.
  GLuint textures_[3];

  static GlVideoRenderer* instance_;

  DISALLOW_COPY_AND_ASSIGN(GlVideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_GL_VIDEO_RENDERER_H_
