// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace content {

scoped_refptr<CompositingIOSurfaceContext>
    CompositingIOSurfaceContext::Get(
        CompositingIOSurfaceMac::SurfaceOrder surface_order) {
  std::vector<NSOpenGLPixelFormatAttribute> attributes;
  attributes.push_back(NSOpenGLPFADoubleBuffer);
  // We don't need a depth buffer - try setting its size to 0...
  attributes.push_back(NSOpenGLPFADepthSize); attributes.push_back(0);
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus())
    attributes.push_back(NSOpenGLPFAAllowOfflineRenderers);
  attributes.push_back(0);

  scoped_nsobject<NSOpenGLPixelFormat> glPixelFormat(
      [[NSOpenGLPixelFormat alloc] initWithAttributes:&attributes.front()]);
  if (!glPixelFormat) {
    LOG(ERROR) << "NSOpenGLPixelFormat initWithAttributes failed";
    return NULL;
  }

  scoped_nsobject<NSOpenGLContext> nsgl_context(
      [[NSOpenGLContext alloc] initWithFormat:glPixelFormat
                                 shareContext:nil]);
  if (!nsgl_context) {
    LOG(ERROR) << "NSOpenGLContext initWithFormat failed";
    return NULL;
  }

  // If requested, ask the WindowServer to render the OpenGL surface underneath
  // the window. This, combined with a hole punched in the window, will allow
  // for views to "overlap" the GL surface from the user's point of view.
  if (surface_order == CompositingIOSurfaceMac::SURFACE_ORDER_BELOW_WINDOW) {
    GLint gl_surface_order = -1;
    [nsgl_context setValues:&gl_surface_order
                  forParameter:NSOpenGLCPSurfaceOrder];
  }

  CGLContextObj cgl_context = (CGLContextObj)[nsgl_context CGLContextObj];
  if (!cgl_context) {
    LOG(ERROR) << "CGLContextObj failed";
    return NULL;
  }

  // Draw at beam vsync.
  bool is_vsync_disabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync);
  GLint swapInterval = is_vsync_disabled ? 0 : 1;
  [nsgl_context setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

  // Prepare the shader program cache.  Precompile only the shader programs
  // needed to draw the IO Surface.
  CGLSetCurrentContext(cgl_context);
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache(
      new CompositingIOSurfaceShaderPrograms());
  const bool prepared = (
      shader_program_cache->UseBlitProgram() &&
      shader_program_cache->UseSolidWhiteProgram());
  glUseProgram(0u);
  CGLSetCurrentContext(0);
  if (!prepared) {
    LOG(ERROR) << "IOSurface failed to compile/link required shader programs.";
    return NULL;
  }

  return new CompositingIOSurfaceContext(
      surface_order,
      nsgl_context.release(),
      cgl_context,
      is_vsync_disabled,
      shader_program_cache.Pass());
}

CompositingIOSurfaceContext::CompositingIOSurfaceContext(
    CompositingIOSurfaceMac::SurfaceOrder surface_order,
    NSOpenGLContext* nsgl_context,
    CGLContextObj cgl_context,
    bool is_vsync_disabled,
    scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache)
    : surface_order_(surface_order),
      nsgl_context_(nsgl_context),
      cgl_context_(cgl_context),
      shader_program_cache_(shader_program_cache.Pass()) {
}

CompositingIOSurfaceContext::~CompositingIOSurfaceContext() {
  CGLSetCurrentContext(cgl_context_);
  shader_program_cache_->Reset();
  CGLSetCurrentContext(0);
}

}  // namespace content
