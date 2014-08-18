// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_

#include <deque>
#include <list>
#include <vector>

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>
#include <QuartzCore/QuartzCore.h>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/video_frame.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace gfx {
class Rect;
}

namespace content {

class CompositingIOSurfaceContext;
class CompositingIOSurfaceShaderPrograms;
class CompositingIOSurfaceTransformer;
class RenderWidgetHostViewFrameSubscriber;
class RenderWidgetHostViewMac;

// This class manages an OpenGL context and IOSurface for the accelerated
// compositing code path. The GL context is attached to
// RenderWidgetHostViewCocoa for blitting the IOSurface.
class CompositingIOSurfaceMac
    : public base::RefCounted<CompositingIOSurfaceMac> {
 public:
  // Returns NULL if IOSurface or GL API calls fail.
  static scoped_refptr<CompositingIOSurfaceMac> Create();

  // Set IOSurface that will be drawn on the next NSView drawRect.
  bool SetIOSurfaceWithContextCurrent(
      scoped_refptr<CompositingIOSurfaceContext> current_context,
      IOSurfaceID io_surface_handle,
      const gfx::Size& size,
      float scale_factor) WARN_UNUSED_RESULT;

  // Get the CGL renderer ID currently associated with this context.
  int GetRendererID();

  // Blit the IOSurface to the rectangle specified by |window_rect| in DIPs,
  // with the origin in the lower left corner. If the window rect's size is
  // larger than the IOSurface, the remaining right and bottom edges will be
  // white. |window_scale_factor| is 1 in normal views, 2 in HiDPI views.
  bool DrawIOSurface(
      scoped_refptr<CompositingIOSurfaceContext> drawing_context,
      const gfx::Rect& window_rect,
      float window_scale_factor) WARN_UNUSED_RESULT;

  // Copy the data of the "live" OpenGL texture referring to this IOSurfaceRef
  // into |out|. The copied region is specified with |src_pixel_subrect| and
  // the data is transformed so that it fits in |dst_pixel_size|.
  // |src_pixel_subrect| and |dst_pixel_size| are not in DIP but in pixel.
  // Caller must ensure that |out| is allocated to dimensions that match
  // dst_pixel_size, with no additional padding.
  // |callback| is invoked when the operation is completed or failed.
  // Do no call this method again before |callback| is invoked.
  void CopyTo(const gfx::Rect& src_pixel_subrect,
              const gfx::Size& dst_pixel_size,
              const base::Callback<void(bool, const SkBitmap&)>& callback);

  // Transfer the contents of the surface to an already-allocated YV12
  // VideoFrame, and invoke a callback to indicate success or failure.
  void CopyToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Unref the IOSurface and delete the associated GL texture. If the GPU
  // process is no longer referencing it, this will delete the IOSurface.
  void UnrefIOSurface();

  bool HasIOSurface() { return !!io_surface_.get(); }

  const gfx::Size& pixel_io_surface_size() const {
    return pixel_io_surface_size_;
  }
  // In cocoa view units / DIPs.
  const gfx::Size& dip_io_surface_size() const { return dip_io_surface_size_; }
  float scale_factor() const { return scale_factor_; }

  // Returns true if asynchronous readback is supported on this system.
  bool IsAsynchronousReadbackSupported();

  // Scan the list of started asynchronous copies and test if each one has
  // completed. If |block_until_finished| is true, then block until all
  // pending copies are finished.
  void CheckIfAllCopiesAreFinished(bool block_until_finished);

  // Returns true if the offscreen context used by this surface has been
  // poisoned.
  bool HasBeenPoisoned() const;

 private:
  friend class base::RefCounted<CompositingIOSurfaceMac>;

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

  CompositingIOSurfaceMac(
      const scoped_refptr<CompositingIOSurfaceContext>& context);
  ~CompositingIOSurfaceMac();

  // Returns true if IOSurface is ready to render. False otherwise.
  bool MapIOSurfaceToTextureWithContextCurrent(
      const scoped_refptr<CompositingIOSurfaceContext>& current_context,
      const gfx::Size pixel_size,
      float scale_factor,
      IOSurfaceID io_surface_handle) WARN_UNUSED_RESULT;

  void UnrefIOSurfaceWithContextCurrent();

  void DrawQuad(const SurfaceQuad& quad);

  // Check for GL errors and store the result in error_. Only return new
  // errors
  GLenum GetAndSaveGLError();

  // Offscreen context used for all operations other than drawing to the
  // screen. This is in the same share group as the contexts used for
  // drawing, and is the same for all IOSurfaces in all windows.
  scoped_refptr<CompositingIOSurfaceContext> offscreen_context_;

  // IOSurface data.
  IOSurfaceID io_surface_handle_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  // The width and height of the io surface.
  gfx::Size pixel_io_surface_size_;  // In pixels.
  gfx::Size dip_io_surface_size_;  // In view / density independent pixels.
  float scale_factor_;

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  // Error saved by GetAndSaveGLError
  GLint gl_error_;

  // Aggressive IOSurface eviction logic. When using CoreAnimation, IOSurfaces
  // are used only transiently to transfer from the GPU process to the browser
  // process. Once the IOSurface has been drawn to its CALayer, the CALayer
  // will not need updating again until its view is hidden and re-shown.
  // Aggressively evict surfaces when more than 8 (the number allowed by the
  // memory manager for fast tab switching) are allocated.
  enum {
    kMaximumUnevictedSurfaces = 8,
  };
  typedef std::list<CompositingIOSurfaceMac*> EvictionQueue;
  void EvictionMarkUpdated();
  void EvictionMarkEvicted();
  EvictionQueue::iterator eviction_queue_iterator_;
  bool eviction_has_been_drawn_since_updated_;

  static void EvictionScheduleDoEvict();
  static void EvictionDoEvict();
  static base::LazyInstance<EvictionQueue> eviction_queue_;
  static bool eviction_scheduled_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_
