// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_

#include <GL/glew.h>
#include <GL/glxew.h>
#include <X11/Xlib.h>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "media/base/factory.h"
#include "media/filters/video_renderer_base.h"

class X11VideoRenderer : public media::VideoRendererBase {
 public:
  static media::FilterFactory* CreateFactory(Display* display,
                                             Window window) {
    return new media::FilterFactoryImpl2<
        X11VideoRenderer, Display*, Window>(display, window);
  }

  X11VideoRenderer(Display* display, Window window);

  // This method is called to paint the current video frame to the assigned
  // window.
  void Paint();

  // media::FilterFactoryImpl2 Implementation.
  static bool IsMediaFormatSupported(const media::MediaFormat& media_format);

  // Returns the instance of this class.
  static X11VideoRenderer* instance() { return instance_; }

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder);
  virtual void OnStop();
  virtual void OnFrameAvailable();

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<X11VideoRenderer>;
  virtual ~X11VideoRenderer();

  int width_;
  int height_;

  Display* display_;
  Window window_;

  // Image in heap that contains the RGBA data of the video frame.
  XImage* image_;

  // Protects |new_frame_|.
  Lock lock_;
  bool new_frame_;

  // Picture represents the paint target. This is a picture located
  // in the server.
  unsigned long picture_;

  bool use_render_;

  bool use_gl_;

  // GL context.
  GLXContext gl_context_;

  // 3 textures, one for each plane.
  GLuint textures_[3];

  // Shaders and program for YUV->RGB conversion.
  GLuint vertex_shader_;
  GLuint fragment_shader_;
  GLuint program_;

  static X11VideoRenderer* instance_;

  DISALLOW_COPY_AND_ASSIGN(X11VideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
