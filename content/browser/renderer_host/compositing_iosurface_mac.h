// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H
#define CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H

#import <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class IOSurfaceSupport;

// This class manages an OpenGL context and IOSurface for the accelerated
// compositing code path. The GL context is attached to
// RenderWidgetHostViewCocoa for blitting the IOSurface.
class CompositingIOSurfaceMac {
 public:
  // Returns NULL if IOSurface support is missing or GL APIs fail.
  static CompositingIOSurfaceMac* Create();
  ~CompositingIOSurfaceMac();

  // Set IOSurface that will be drawn on the next NSView drawRect.
  void SetIOSurface(uint64 io_surface_handle);

  // Blit the IOSurface at the upper-left corner of the |view|. If |view| window
  // size is larger than the IOSurface, the remaining left and bottom edges will
  // be white.
  void DrawIOSurface(NSView* view);

  // Copy the data of the "live" OpenGL texture referring to this IOSurfaceRef
  // into |out|. The image data is transformed so that it fits in |dst_size|.
  // Caller must ensure that |out| is allocated with the size no less than
  // |4 * dst_size.width() * dst_size.height()| bytes.
  bool CopyTo(const gfx::Size& dst_size, void* out);

  // Unref the IOSurface and delete the associated GL texture. If the GPU
  // process is no longer referencing it, this will delete the IOSurface.
  void UnrefIOSurface();

  // Call when globalFrameDidChange is received on the NSView.
  void GlobalFrameDidChange();

  // Disassociate the GL context with the NSView and unref the IOSurface. Do
  // this to switch to software drawing mode.
  void ClearDrawable();

  bool HasIOSurface() { return !!io_surface_.get(); }

  const gfx::Size& io_surface_size() const { return io_surface_size_; }

 private:
  // Vertex structure for use in glDraw calls.
  struct SurfaceVertex {
    SurfaceVertex() : x_(0.0f), y_(0.0f), tx_(0.0f), ty_(0.0f) { }
    // Currently the texture coords are always the same as vertex coords.
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
    SurfaceVertex verts_[4];
  };

  CompositingIOSurfaceMac(IOSurfaceSupport* io_surface_support,
                          NSOpenGLContext* glContext,
                          CGLContextObj cglContext,
                          GLuint shader_program_blit_rgb,
                          GLint blit_rgb_sampler_location,
                          GLuint shader_program_white);

  // Returns true if IOSurface is ready to render. False otherwise.
  bool MapIOSurfaceToTexture(uint64 io_surface_handle);

  void UnrefIOSurfaceWithContextCurrent();

  void DrawQuad(const SurfaceQuad& quad);

  // Cached pointer to IOSurfaceSupport Singleton.
  IOSurfaceSupport* io_surface_support_;

  // GL context
  scoped_nsobject<NSOpenGLContext> glContext_;
  CGLContextObj cglContext_;  // weak, backed by |glContext_|.

  // IOSurface data.
  uint64 io_surface_handle_;
  base::mac::ScopedCFTypeRef<CFTypeRef> io_surface_;

  // The width and height of the io surface.
  gfx::Size io_surface_size_;

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  // Shader parameters.
  GLuint shader_program_blit_rgb_;
  GLint blit_rgb_sampler_location_;
  GLuint shader_program_white_;

  SurfaceQuad quad_;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H
