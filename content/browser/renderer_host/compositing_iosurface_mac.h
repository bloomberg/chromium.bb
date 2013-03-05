// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H
#define CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CVDisplayLink.h>
#include <QuartzCore/QuartzCore.h>

#include "base/callback.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/timer.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size.h"

class IOSurfaceSupport;

namespace gfx {
class Rect;
}

namespace content {

class RenderWidgetHostViewFrameSubscriber;

// This class manages an OpenGL context and IOSurface for the accelerated
// compositing code path. The GL context is attached to
// RenderWidgetHostViewCocoa for blitting the IOSurface.
class CompositingIOSurfaceMac {
 public:
  // Passed to Create() to specify the ordering of the surface relative to the
  // containing window.
  enum SurfaceOrder {
    SURFACE_ORDER_ABOVE_WINDOW,
    SURFACE_ORDER_BELOW_WINDOW
  };

  // Returns NULL if IOSurface support is missing or GL APIs fail. Specify in
  // |order| the desired ordering relationship of the surface to the containing
  // window.
  static CompositingIOSurfaceMac* Create(SurfaceOrder order);
  ~CompositingIOSurfaceMac();

  // Set IOSurface that will be drawn on the next NSView drawRect.
  void SetIOSurface(uint64 io_surface_handle,
                    const gfx::Size& size);

  // Blit the IOSurface at the upper-left corner of the |view|. If |view| window
  // size is larger than the IOSurface, the remaining right and bottom edges
  // will be white. |scaleFactor| is 1 in normal views, 2 in HiDPI views.
  // |frame_subscriber| listens to this draw event and provides output buffer
  // for copying this frame into.
  void DrawIOSurface(NSView* view, float scale_factor,
                     RenderWidgetHostViewFrameSubscriber* frame_subscriber);

  // Copy the data of the "live" OpenGL texture referring to this IOSurfaceRef
  // into |out|. The copied region is specified with |src_pixel_subrect| and
  // the data is transformed so that it fits in |dst_pixel_size|.
  // |src_pixel_subrect| and |dst_pixel_size| are not in DIP but in pixel.
  // Caller must ensure that |out| is allocated to dimensions that match
  // dst_pixel_size, with no additional padding.
  // |callback| is invoked when the operation is completed or failed.
  // Do no call this method again before |callback| is invoked.
  void CopyTo(const gfx::Rect& src_pixel_subrect,
              float src_scale_factor,
              const gfx::Size& dst_pixel_size,
              const SkBitmap& out,
              const base::Callback<void(bool, const SkBitmap&)>& callback);

  // Transfer the contents of the surface to an already-allocated YV12
  // VideoFrame, and invoke a callback to indicate success or failure.
  void CopyToVideoFrame(
      const gfx::Rect& src_subrect,
      float src_scale_factor,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Unref the IOSurface and delete the associated GL texture. If the GPU
  // process is no longer referencing it, this will delete the IOSurface.
  void UnrefIOSurface();

  // Call when globalFrameDidChange is received on the NSView.
  void GlobalFrameDidChange();

  // Disassociate the GL context with the NSView and unref the IOSurface. Do
  // this to switch to software drawing mode.
  void ClearDrawable();

  bool HasIOSurface() { return !!io_surface_.get(); }

  const gfx::Size& pixel_io_surface_size() const {
    return pixel_io_surface_size_;
  }
  // In cocoa view units / DIPs.
  const gfx::Size& io_surface_size() const { return io_surface_size_; }

  bool is_vsync_disabled() const { return is_vsync_disabled_; }

  // Get vsync scheduling parameters.
  // |interval_numerator/interval_denominator| equates to fractional number of
  // seconds between vsyncs.
  void GetVSyncParameters(base::TimeTicks* timebase,
                          uint32* interval_numerator,
                          uint32* interval_denominator);

 private:
  friend CVReturn DisplayLinkCallback(CVDisplayLinkRef,
                                      const CVTimeStamp*,
                                      const CVTimeStamp*,
                                      CVOptionFlags,
                                      CVOptionFlags*,
                                      void*);

  // Vertex structure for use in glDraw calls.
  struct SurfaceVertex {
    SurfaceVertex() : x_(0.0f), y_(0.0f), tx_(0.0f), ty_(0.0f) { }
    void set(float x, float y, float tx, float ty) {
      x_ = x;
      y_ = y;
      tx_ = tx;
      ty_ = ty;
    }
    void set_position(float x, float y) {
      x_ = x;
      y_ = y;
    }
    void set_texcoord(float tx, float ty) {
      tx_ = tx;
      ty_ = ty;
    }
    float x_;
    float y_;
    float tx_;
    float ty_;
  };

  // Counter-clockwise verts starting from upper-left corner (0, 0).
  struct SurfaceQuad {
    void set_size(gfx::Size vertex_size, gfx::Size texcoord_size) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      float vw = static_cast<float>(vertex_size.width());
      float vh = static_cast<float>(vertex_size.height());
      float tw = static_cast<float>(texcoord_size.width());
      float th = static_cast<float>(texcoord_size.height());
      verts_[0].set(0.0f, 0.0f, 0.0f, th);
      verts_[1].set(0.0f, vh, 0.0f, 0.0f);
      verts_[2].set(vw, vh, tw, 0.0f);
      verts_[3].set(vw, 0.0f, tw, th);
    }
    void set_rect(float x1, float y1, float x2, float y2) {
      verts_[0].set_position(x1, y1);
      verts_[1].set_position(x1, y2);
      verts_[2].set_position(x2, y2);
      verts_[3].set_position(x2, y1);
    }
    void set_texcoord_rect(float tx1, float ty1, float tx2, float ty2) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      verts_[0].set_texcoord(tx1, ty2);
      verts_[1].set_texcoord(tx1, ty1);
      verts_[2].set_texcoord(tx2, ty1);
      verts_[3].set_texcoord(tx2, ty2);
    }
    SurfaceVertex verts_[4];
  };

  // Keeps track of states and buffers for asynchronous readback of IOSurface.
  struct CopyContext {
    CopyContext();
    ~CopyContext();

    void Reset() {
      started = false;
      cycles_elapsed = 0;
      frame_buffer = 0;
      frame_buffer_texture = 0;
      pixel_buffer = 0;
      use_fence = false;
      fence = 0;
      map_buffer_callback.Reset();
    }

    bool started;
    int cycles_elapsed;
    GLuint frame_buffer;
    GLuint frame_buffer_texture;
    GLuint pixel_buffer;
    bool use_fence;
    GLuint fence;
    gfx::Rect src_rect;
    gfx::Size dest_size;
    base::Callback<base::Closure(void*)> map_buffer_callback;
  };

  CompositingIOSurfaceMac(IOSurfaceSupport* io_surface_support,
                          NSOpenGLContext* glContext,
                          CGLContextObj cglContext,
                          GLuint shader_program_blit_rgb,
                          GLint blit_rgb_sampler_location,
                          GLuint shader_program_white,
                          bool is_vsync_disabled,
                          CVDisplayLinkRef display_link);

  bool IsVendorIntel();

  // Returns true if IOSurface is ready to render. False otherwise.
  bool MapIOSurfaceToTexture(uint64 io_surface_handle);

  void UnrefIOSurfaceWithContextCurrent();

  void DrawQuad(const SurfaceQuad& quad);

  // Called on display-link thread.
  void DisplayLinkTick(CVDisplayLinkRef display_link,
                       const CVTimeStamp* time);

  void CalculateVsyncParametersLockHeld(const CVTimeStamp* time);

  // Prevent from spinning on CGLFlushDrawable when it fails to throttle to
  // VSync frequency.
  void RateLimitDraws();

  void StartOrContinueDisplayLink();
  void StopDisplayLink();

  // Copy current frame to |target| video frame. This method must be called
  // within a CGL context. Returns a callback that should be called outside
  // of the CGL context.
  base::Closure CopyToVideoFrameInternal(
      const gfx::Rect& src_subrect,
      float src_scale_factor,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Two implementations of CopyTo() in synchronous and asynchronous mode.
  // These may copy regions smaller than the requested |src_pixel_subrect| if
  // the iosurface is smaller than |src_pixel_subrect|.
  bool SynchronousCopyTo(const gfx::Rect& src_pixel_subrect,
                         float src_scale_factor,
                         const gfx::Size& dst_pixel_size,
                         const SkBitmap& out);
  bool AsynchronousCopyTo(
      const gfx::Rect& src_pixel_subrect,
      float src_scale_factor,
      const gfx::Size& dst_pixel_size,
      const base::Callback<base::Closure(void*)>& map_buffer_callback);
  void FinishCopy();
  void CleanupResourcesForCopy();

  gfx::Rect IntersectWithIOSurface(const gfx::Rect& rect,
                                   float scale_factor) const;

  // Cached pointer to IOSurfaceSupport Singleton.
  IOSurfaceSupport* io_surface_support_;

  // GL context
  scoped_nsobject<NSOpenGLContext> glContext_;
  CGLContextObj cglContext_;  // weak, backed by |glContext_|.

  // IOSurface data.
  uint64 io_surface_handle_;
  base::mac::ScopedCFTypeRef<CFTypeRef> io_surface_;

  // The width and height of the io surface.
  gfx::Size pixel_io_surface_size_;  // In pixels.
  gfx::Size io_surface_size_;  // In view units.

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  CopyContext copy_context_;

  // Timer for finishing a copy operation.
  base::RepeatingTimer<CompositingIOSurfaceMac> copy_timer_;

  // Shader parameters.
  GLuint shader_program_blit_rgb_;
  GLint blit_rgb_sampler_location_;
  GLuint shader_program_white_;

  SurfaceQuad quad_;

  bool is_vsync_disabled_;

  // CVDisplayLink for querying Vsync timing info and throttling swaps.
  CVDisplayLinkRef display_link_;

  // Timer for stopping display link after a timeout with no swaps.
  base::DelayTimer<CompositingIOSurfaceMac> display_link_stop_timer_;

  // Lock for sharing data between UI thread and display-link thread.
  base::Lock lock_;

  // Counts for throttling swaps.
  int64 vsync_count_;
  int64 swap_count_;

  // Vsync timing data.
  base::TimeTicks vsync_timebase_;
  uint32 vsync_interval_numerator_;
  uint32 vsync_interval_denominator_;

  bool initialized_is_intel_;
  bool is_intel_;
  GLint screen_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H
