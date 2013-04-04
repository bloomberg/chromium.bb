// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_

#import <AppKit/NSOpenGL.h>
#include <OpenGL/OpenGL.h>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"

namespace content {

class CompositingIOSurfaceShaderPrograms;

class CompositingIOSurfaceContext
    : public base::RefCounted<CompositingIOSurfaceContext> {
 public:
  static scoped_refptr<CompositingIOSurfaceContext> Get(
      CompositingIOSurfaceMac::SurfaceOrder surface_order);

  CompositingIOSurfaceShaderPrograms* shader_program_cache() const {
    return shader_program_cache_.get();
  }
  NSOpenGLContext* nsgl_context() const { return nsgl_context_; }
  CGLContextObj cgl_context() const { return cgl_context_; }
  bool is_vsync_disabled() const { return is_vsync_disabled_; }

 private:
  friend class base::RefCounted<CompositingIOSurfaceContext>;

  CompositingIOSurfaceContext(
      CompositingIOSurfaceMac::SurfaceOrder surface_order,
      NSOpenGLContext* nsgl_context,
      CGLContextObj clg_context,
      bool is_vsync_disabled_,
      scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache);
  ~CompositingIOSurfaceContext();

  // The value for NSOpenGLCPSurfaceOrder for this GL context,  1 will
  // render above the window and -1 will render below the window.
  GLint surface_order_;
  scoped_nsobject<NSOpenGLContext> nsgl_context_;
  CGLContextObj cgl_context_; // weak, backed by |nsgl_context_|
  bool is_vsync_disabled_;
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_
