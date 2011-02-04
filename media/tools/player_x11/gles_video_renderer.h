// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_GLES_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_GLES_VIDEO_RENDERER_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <utility>
#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"

class GlesVideoRenderer : public media::VideoRendererBase {
 public:
  GlesVideoRenderer(Display* display, Window window, MessageLoop* message_loop);

  // This method is called to paint the current video frame to the assigned
  // window.
  void Paint();

  static GlesVideoRenderer* instance() { return instance_; }

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
  friend class scoped_refptr<GlesVideoRenderer>;
  virtual ~GlesVideoRenderer();

  GLuint FindTexture(scoped_refptr<media::VideoFrame> video_frame);
  bool InitializeGles();
  void CreateShader(GLuint program, GLenum type,
                    const char* vs_source, int vs_size);
  void LinkProgram(GLuint program);
  void CreateTextureAndProgramEgl();
  void CreateTextureAndProgramYuv2Rgb();

  void DeInitializeGlesTask(media::FilterCallback* callback);

  PFNEGLCREATEIMAGEKHRPROC egl_create_image_khr_;
  PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image_khr_;

  Display* display_;
  Window window_;

  // EGL context.
  EGLDisplay egl_display_;
  EGLSurface egl_surface_;
  EGLContext egl_context_;

  // textures for EGL image
  typedef std::pair<scoped_refptr<media::VideoFrame>, GLuint> EglFrame;
  std::vector<EglFrame> egl_frames_;

  // 3 textures, one for each plane.
  GLuint textures_[3];

  MessageLoop* glx_thread_message_loop_;
  static GlesVideoRenderer* instance_;

  DISALLOW_COPY_AND_ASSIGN(GlesVideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_GLES_VIDEO_RENDERER_H_
