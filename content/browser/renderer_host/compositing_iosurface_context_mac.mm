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
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace content {

CoreAnimationStatus GetCoreAnimationStatus() {
  static CoreAnimationStatus status =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCoreAnimation) ?
              CORE_ANIMATION_DISABLED : CORE_ANIMATION_ENABLED;
  return status;
}

// static
scoped_refptr<CompositingIOSurfaceContext>
CompositingIOSurfaceContext::Get(int window_number) {
  TRACE_EVENT0("browser", "CompositingIOSurfaceContext::Get");

  // Return the context for this window_number, if it exists.
  WindowMap::iterator found = window_map()->find(window_number);
  if (found != window_map()->end()) {
    DCHECK(!found->second->poisoned_);
    return found->second;
  }

  static bool is_vsync_disabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableGpuVsync);

  base::scoped_nsobject<NSOpenGLContext> nsgl_context;
  base::ScopedTypeRef<CGLContextObj> cgl_context_strong;
  CGLContextObj cgl_context = NULL;
  if (GetCoreAnimationStatus() == CORE_ANIMATION_DISABLED) {
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
    nsgl_context.reset(
        [[NSOpenGLContext alloc] initWithFormat:glPixelFormat
                                   shareContext:share_context]);
    if (!nsgl_context) {
      LOG(ERROR) << "NSOpenGLContext initWithFormat failed";
      return NULL;
    }

    // Grab the CGL context that the NSGL context is using. Explicitly
    // retain it, so that it is not double-freed by the scoped type.
    cgl_context = reinterpret_cast<CGLContextObj>(
        [nsgl_context CGLContextObj]);
    if (!cgl_context) {
      LOG(ERROR) << "Failed to retrieve CGLContextObj from NSOpenGLContext";
      return NULL;
    }

    // Force [nsgl_context flushBuffer] to wait for vsync.
    GLint swapInterval = is_vsync_disabled ? 0 : 1;
    [nsgl_context setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
  } else {
    CGLError error = kCGLNoError;

    // Create the pixel format object for the context.
    std::vector<CGLPixelFormatAttribute> attribs;
    attribs.push_back(kCGLPFADepthSize);
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
    if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
      attribs.push_back(kCGLPFAAllowOfflineRenderers);
      attribs.push_back(static_cast<CGLPixelFormatAttribute>(1));
    }
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));
    GLint number_virtual_screens = 0;
    base::ScopedTypeRef<CGLPixelFormatObj> pixel_format;
    error = CGLChoosePixelFormat(&attribs.front(),
                                 pixel_format.InitializeInto(),
                                 &number_virtual_screens);
    if (error != kCGLNoError) {
      LOG(ERROR) << "Failed to create pixel format object.";
      return NULL;
    }

    // Create all contexts in the same share group so that the textures don't
    // need to be recreated when transitioning contexts.
    CGLContextObj share_context = NULL;
    if (!window_map()->empty())
      share_context = window_map()->begin()->second->cgl_context();
    error = CGLCreateContext(
        pixel_format, share_context, cgl_context_strong.InitializeInto());
    if (error != kCGLNoError) {
      LOG(ERROR) << "Failed to create context object.";
      return NULL;
    }
    cgl_context = cgl_context_strong;

    // Note that VSync is ignored because CoreAnimation will automatically
    // rate limit draws.
  }

  // Prepare the shader program cache. Precompile the shader programs
  // needed to draw the IO Surface for non-offscreen contexts.
  bool prepared = false;
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache;
  {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(cgl_context);
    shader_program_cache.reset(new CompositingIOSurfaceShaderPrograms());
    if (window_number == kOffscreenContextWindowNumber) {
      prepared = true;
    } else {
      prepared = (
          shader_program_cache->UseBlitProgram() &&
          shader_program_cache->UseSolidWhiteProgram());
    }
    glUseProgram(0u);
  }
  if (!prepared) {
    LOG(ERROR) << "IOSurface failed to compile/link required shader programs.";
    return NULL;
  }

  return new CompositingIOSurfaceContext(
      window_number,
      nsgl_context.release(),
      cgl_context_strong,
      cgl_context,
      is_vsync_disabled,
      shader_program_cache.Pass());
}

void CompositingIOSurfaceContext::PoisonContextAndSharegroup() {
  if (poisoned_)
    return;

  for (WindowMap::iterator it = window_map()->begin();
       it != window_map()->end();
       ++it) {
    it->second->poisoned_ = true;
  }
  window_map()->clear();
}

CompositingIOSurfaceContext::CompositingIOSurfaceContext(
    int window_number,
    NSOpenGLContext* nsgl_context,
    base::ScopedTypeRef<CGLContextObj> cgl_context_strong,
    CGLContextObj cgl_context,
    bool is_vsync_disabled,
    scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache)
    : window_number_(window_number),
      nsgl_context_(nsgl_context),
      cgl_context_strong_(cgl_context_strong),
      cgl_context_(cgl_context),
      is_vsync_disabled_(is_vsync_disabled),
      shader_program_cache_(shader_program_cache.Pass()),
      poisoned_(false) {
  DCHECK(window_map()->find(window_number_) == window_map()->end());
  window_map()->insert(std::make_pair(window_number_, this));
}

CompositingIOSurfaceContext::~CompositingIOSurfaceContext() {
  {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(cgl_context_);
    shader_program_cache_->Reset();
  }
  if (!poisoned_) {
    DCHECK(window_map()->find(window_number_) != window_map()->end());
    DCHECK(window_map()->find(window_number_)->second == this);
    window_map()->erase(window_number_);
  } else {
    WindowMap::const_iterator found = window_map()->find(window_number_);
    if (found != window_map()->end())
      DCHECK(found->second != this);
  }
}

NSOpenGLContext* CompositingIOSurfaceContext::nsgl_context() const {
  // This should not be called from any CoreAnimation paths.
  CHECK(GetCoreAnimationStatus() == CORE_ANIMATION_DISABLED);
  return nsgl_context_;
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
