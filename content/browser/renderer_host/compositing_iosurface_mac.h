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
    float x_;
    float y_;
    float tx_;
    float ty_;
  };

  // Counter-clockwise verts starting from upper-left corner (0, 0).
  struct SurfaceQuad {
    void set_size(gfx::Size size) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      float w = static_cast<float>(size.width());
      float h = static_cast<float>(size.height());
      verts_[0].set(0.0f, 0.0f, 0.0f, h);
      verts_[1].set(0.0f, h, 0.0f, 0.0f);
      verts_[2].set(w, h, w, 0.0f);
      verts_[3].set(w, 0.0f, w, h);
    }
    SurfaceVertex verts_[4];
  };

  CompositingIOSurfaceMac(IOSurfaceSupport* io_surface_support,
                          NSOpenGLContext* glContext,
                          CGLContextObj cglContext);

  // Returns true if IOSurface is ready to render. False otherwise.
  bool MapIOSurfaceToTexture(uint64 io_surface_handle);

  void UnrefIOSurfaceWithContextCurrent();

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

  SurfaceQuad quad_;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_ACCELERATED_COMPOSITING_VIEW_MAC_H
