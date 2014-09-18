// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
#define CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_

#include <map>
#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace base {
class MessageLoop;
class WaitableEvent;
}

namespace content {

class VideoFrameTexture : public base::RefCounted<VideoFrameTexture> {
 public:
  uint32 texture_id() const { return texture_id_; }
  uint32 texture_target() const { return texture_target_; }

  VideoFrameTexture(uint32 texture_target,
                    uint32 texture_id,
                    const base::Closure& no_longer_needed_cb);

 private:
  friend class base::RefCounted<VideoFrameTexture>;

  uint32 texture_target_;
  uint32 texture_id_;
  base::Closure no_longer_needed_cb_;

  ~VideoFrameTexture();
};

struct RenderingHelperParams {
  RenderingHelperParams();
  ~RenderingHelperParams();

  // The rendering FPS.
  int rendering_fps;

  // The desired size of each window. We play each stream in its own window
  // on the screen.
  std::vector<gfx::Size> window_sizes;

  // The members below are only used for the thumbnail mode where all frames
  // are rendered in sequence onto one FBO for comparison/verification purposes.

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

  RenderingHelper();
  ~RenderingHelper();

  static bool InitializeOneOff();

  // Create the render context and windows by the specified dimensions.
  void Initialize(const RenderingHelperParams& params,
                  base::WaitableEvent* done);

  // Undo the effects of Initialize() and signal |*done|.
  void UnInitialize(base::WaitableEvent* done);

  // Return a newly-created GLES2 texture id of the specified size, and
  // signal |*done|.
  void CreateTexture(uint32 texture_target,
                     uint32* texture_id,
                     const gfx::Size& size,
                     base::WaitableEvent* done);

  // Render thumbnail in the |texture_id| to the FBO buffer using target
  // |texture_target|.
  void RenderThumbnail(uint32 texture_target, uint32 texture_id);

  // Queues the |video_frame| for rendering.
  void QueueVideoFrame(size_t window_id,
                       scoped_refptr<VideoFrameTexture> video_frame);

  // Flushes the pending frames. Notify the rendering_helper there won't be
  // more video frames.
  void Flush(size_t window_id);

  // Delete |texture_id|.
  void DeleteTexture(uint32 texture_id);

  // Get the platform specific handle to the OpenGL display.
  void* GetGLDisplay();

  // Get the platform specific handle to the OpenGL context.
  void* GetGLContext();

  // Get rendered thumbnails as RGB.
  // Sets alpha_solid to true if the alpha channel is entirely 0xff.
  void GetThumbnailsAsRGB(std::vector<unsigned char>* rgb,
                          bool* alpha_solid,
                          base::WaitableEvent* done);

 private:
  struct RenderedVideo {
    // The rect on the screen where the video will be rendered.
    gfx::Rect render_area;

    // True if the last (and the only one) frame in pending_frames has
    // been rendered. We keep the last remaining frame in pending_frames even
    // after it has been rendered, so that we have something to display if the
    // client is falling behind on providing us with new frames during
    // timer-driven playback.
    bool last_frame_rendered;

    // True if there won't be any new video frames comming.
    bool is_flushing;

    // The number of frames need to be dropped to catch up the rendering.
    int frames_to_drop;

    // The video frames pending for rendering.
    std::queue<scoped_refptr<VideoFrameTexture> > pending_frames;

    RenderedVideo();
    ~RenderedVideo();
  };

  void Clear();

  void RenderContent();

  void LayoutRenderingAreas(const std::vector<gfx::Size>& window_sizes);

  // Render |texture_id| to the current view port of the screen using target
  // |texture_target|.
  void RenderTexture(uint32 texture_target, uint32 texture_id);

  base::MessageLoop* message_loop_;

  scoped_refptr<gfx::GLContext> gl_context_;
  scoped_refptr<gfx::GLSurface> gl_surface_;

  gfx::AcceleratedWidget window_;

  gfx::Size screen_size_;

  std::vector<RenderedVideo> videos_;

  bool render_as_thumbnails_;
  int frame_count_;
  GLuint thumbnails_fbo_id_;
  GLuint thumbnails_texture_id_;
  gfx::Size thumbnails_fbo_size_;
  gfx::Size thumbnail_size_;
  GLuint program_;
  base::TimeDelta frame_duration_;
  base::TimeTicks scheduled_render_time_;
  base::CancelableClosure render_task_;

  DISALLOW_COPY_AND_ASSIGN(RenderingHelper);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
