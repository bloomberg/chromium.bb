// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace content {

// static
scoped_refptr<CompositingIOSurfaceContext>
CompositingIOSurfaceContext::Get(int window_number) {
  TRACE_EVENT0("browser", "CompositingIOSurfaceContext::Get");

  // Return the context for this window_number, if it exists.
  WindowMap::iterator found = window_map()->find(window_number);
  if (found != window_map()->end()) {
    DCHECK(found->second->can_be_shared_);
    return found->second;
  }

  std::vector<NSOpenGLPixelFormatAttribute> attributes;
  attributes.push_back(NSOpenGLPFADoubleBuffer);
  // We don't need a depth buffer - try setting its size to 0...
  attributes.push_back(NSOpenGLPFADepthSize); attributes.push_back(0);
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus())
    attributes.push_back(NSOpenGLPFAAllowOfflineRenderers);
  attributes.push_back(0);

  base::scoped_nsobject<NSOpenGLPixelFormat> glPixelFormat(
      [[NSOpenGLPixelFormat alloc] initWithAttributes:&attributes.front()]);
  if (!glPixelFormat) {
    LOG(ERROR) << "NSOpenGLPixelFormat initWithAttributes failed";
    return NULL;
  }

  // Create all contexts in the same share group so that the textures don't
  // need to be recreated when transitioning contexts.
  NSOpenGLContext* share_context = nil;
  if (!window_map()->empty())
    share_context = window_map()->begin()->second->nsgl_context();
  base::scoped_nsobject<NSOpenGLContext> nsgl_context(
      [[NSOpenGLContext alloc] initWithFormat:glPixelFormat
                                 shareContext:share_context]);
  if (!nsgl_context) {
    LOG(ERROR) << "NSOpenGLContext initWithFormat failed";
    return NULL;
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
      window_number,
      nsgl_context.release(),
      cgl_context,
      is_vsync_disabled,
      shader_program_cache.Pass());
}

// static
void CompositingIOSurfaceContext::MarkExistingContextsAsNotShareable() {
  for (WindowMap::iterator it = window_map()->begin();
       it != window_map()->end();
       ++it) {
    it->second->can_be_shared_ = false;
  }
  window_map()->clear();
}

CompositingIOSurfaceContext::CompositingIOSurfaceContext(
    int window_number,
    NSOpenGLContext* nsgl_context,
    CGLContextObj cgl_context,
    bool is_vsync_disabled,
    scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache)
    : window_number_(window_number),
      nsgl_context_(nsgl_context),
      cgl_context_(cgl_context),
      is_vsync_disabled_(is_vsync_disabled),
      shader_program_cache_(shader_program_cache.Pass()),
      can_be_shared_(true) {
  DCHECK(window_map()->find(window_number_) == window_map()->end());
  window_map()->insert(std::make_pair(window_number_, this));
}

CompositingIOSurfaceContext::~CompositingIOSurfaceContext() {
  CGLSetCurrentContext(cgl_context_);
  shader_program_cache_->Reset();
  CGLSetCurrentContext(0);
  if (can_be_shared_) {
    DCHECK(window_map()->find(window_number_) != window_map()->end());
    DCHECK(window_map()->find(window_number_)->second == this);
    window_map()->erase(window_number_);
  } else {
    WindowMap::const_iterator found = window_map()->find(window_number_);
    if (found != window_map()->end())
      DCHECK(found->second != this);
  }
}

// static
CompositingIOSurfaceContext::WindowMap*
    CompositingIOSurfaceContext::window_map() {
  return window_map_.Pointer();
}

// static
base::LazyInstance<CompositingIOSurfaceContext::WindowMap>
    CompositingIOSurfaceContext::window_map_;

}  // namespace content
