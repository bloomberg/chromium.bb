// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
#define CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/gfx/size.h"

namespace base {
class WaitableEvent;
}

namespace content {

struct RenderingHelperParams {
  RenderingHelperParams();
  ~RenderingHelperParams();

  bool suppress_swap_to_display;
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

// Creates and draws textures used by the video decoder.
// This class is not thread safe and thus all the methods of this class
// (except for ctor/dtor) ensure they're being run on a single thread.
class RenderingHelper {
 public:
  // Create a platform specific implementation of this object.
  static RenderingHelper* Create();

  RenderingHelper() {}
  virtual ~RenderingHelper() {}

  // Create the render context and windows by the specified dimensions.
  virtual void Initialize(const RenderingHelperParams& params,
                          base::WaitableEvent* done) = 0;

  // Undo the effects of Initialize() and signal |*done|.
  virtual void UnInitialize(base::WaitableEvent* done) = 0;

  // Return a newly-created GLES2 texture id rendering to a specific window, and
  // signal |*done|.
  virtual void CreateTexture(int window_id,
                             uint32 texture_target,
                             uint32* texture_id,
                             base::WaitableEvent* done) = 0;

  // Render |texture_id| to the screen.
  virtual void RenderTexture(uint32 texture_id) = 0;

  // Delete |texture_id|.
  virtual void DeleteTexture(uint32 texture_id) = 0;

  // Get the platform specific handle to the OpenGL context.
  virtual void* GetGLContext() = 0;

  // Get the platform specific handle to the OpenGL display.
  virtual void* GetGLDisplay() = 0;

  // Get rendered thumbnails as RGB.
  // Sets alpha_solid to true if the alpha channel is entirely 0xff.
  virtual void GetThumbnailsAsRGB(std::vector<unsigned char>* rgb,
                                  bool* alpha_solid,
                                  base::WaitableEvent* done) = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
