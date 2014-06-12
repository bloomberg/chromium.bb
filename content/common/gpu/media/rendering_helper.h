// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
#define CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace base {
class MessageLoop;
class WaitableEvent;
}

#if !defined(OS_WIN) && defined(ARCH_CPU_X86_FAMILY)
#define GL_VARIANT_GLX 1
typedef GLXContext NativeContextType;
#else
#define GL_VARIANT_EGL 1
typedef EGLContext NativeContextType;
#endif

namespace content {

struct RenderingHelperParams;

// Creates and draws textures used by the video decoder.
// This class is not thread safe and thus all the methods of this class
// (except for ctor/dtor) ensure they're being run on a single thread.
class RenderingHelper {
 public:
  // Interface for the content provider of the RenderingHelper.
  class Client {
   public:
    // Callback to tell client to render the content.
    virtual void RenderContent(RenderingHelper* helper) = 0;

   protected:
    virtual ~Client() {}
  };

  RenderingHelper();
  ~RenderingHelper();

  // Create the render context and windows by the specified dimensions.
  void Initialize(const RenderingHelperParams& params,
                  base::WaitableEvent* done);

  // Undo the effects of Initialize() and signal |*done|.
  void UnInitialize(base::WaitableEvent* done);

  // Return a newly-created GLES2 texture id rendering to a specific window, and
  // signal |*done|.
  void CreateTexture(int window_id,
                     uint32 texture_target,
                     uint32* texture_id,
                     base::WaitableEvent* done);

  // Render thumbnail in the |texture_id| to the FBO buffer using target
  // |texture_target|.
  void RenderThumbnail(uint32 texture_target, uint32 texture_id);

  // Render |texture_id| to the current view port of the screen using target
  // |texture_target|.
  void RenderTexture(uint32 texture_target, uint32 texture_id);

  // Delete |texture_id|.
  void DeleteTexture(uint32 texture_id);

  // Get the platform specific handle to the OpenGL display.
  void* GetGLDisplay();

  // Get the platform specific handle to the OpenGL context.
  NativeContextType GetGLContext();

  // Get rendered thumbnails as RGB.
  // Sets alpha_solid to true if the alpha channel is entirely 0xff.
  void GetThumbnailsAsRGB(std::vector<unsigned char>* rgb,
                          bool* alpha_solid,
                          base::WaitableEvent* done);

 private:
  void Clear();

  void RenderContent();

  // Timer to trigger the RenderContent() repeatly.
  base::RepeatingTimer<RenderingHelper> render_timer_;
  base::MessageLoop* message_loop_;
  std::vector<gfx::Size> frame_dimensions_;

  NativeContextType gl_context_;
  std::map<uint32, int> texture_id_to_surface_index_;

#if defined(GL_VARIANT_EGL)
  EGLDisplay gl_display_;
  EGLSurface gl_surface_;
#else
  XVisualInfo* x_visual_;
#endif

#if defined(OS_WIN)
  HWND window_;
#else
  Display* x_display_;
  Window x_window_;
#endif

  // The rendering area of each window on the screen.
  std::vector<gfx::Rect> render_areas_;

  std::vector<base::WeakPtr<Client> > clients_;

  bool render_as_thumbnails_;
  int frame_count_;
  GLuint thumbnails_fbo_id_;
  GLuint thumbnails_texture_id_;
  gfx::Size thumbnails_fbo_size_;
  gfx::Size thumbnail_size_;
  GLuint program_;
  base::TimeDelta frame_duration_;

  DISALLOW_COPY_AND_ASSIGN(RenderingHelper);
};

struct RenderingHelperParams {
  RenderingHelperParams();
  ~RenderingHelperParams();
  int rendering_fps;

  std::vector<base::WeakPtr<RenderingHelper::Client> > clients;
  int num_windows;

  // Dimensions of window(s) created for displaying frames. In the
  // case of thumbnail rendering, these won't match the frame dimensions.
  std::vector<gfx::Size> window_dimensions;
  // Dimensions of video frame texture(s).
  std::vector<gfx::Size> frame_dimensions;
  // Whether the frames are rendered as scaled thumbnails within a
  // larger FBO that is in turn rendered to the window.
  bool render_as_thumbnails;
  // The size of the FBO containing all visible thumbnails.
  gfx::Size thumbnails_page_size;
  // The size of each thumbnail within the FBO.
  gfx::Size thumbnail_size;
};
}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
