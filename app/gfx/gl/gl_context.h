// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GL_GL_CONTEXT_H_
#define APP_GFX_GL_GL_CONTEXT_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {

// Encapsulates an OpenGL context, hiding platform specific management.
class GLContext {
 public:
  GLContext() {}
  virtual ~GLContext() {}

  // Destroys the GL context.
  virtual void Destroy() = 0;

  // Makes the GL context current on the current thread.
  virtual bool MakeCurrent() = 0;

  // Returns true if this context is current.
  virtual bool IsCurrent() = 0;

  // Returns true if this context is offscreen.
  virtual bool IsOffscreen() = 0;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual bool SwapBuffers() = 0;

  // Get the size of the back buffer.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific GL context "handle".
  virtual void* GetHandle() = 0;

  // Set swap interval. This context must be current.
  virtual void SetSwapInterval(int interval) = 0;

  // Returns the internal frame buffer object name if the context is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFrameBufferObject();

  // Returns space separated list of extensions. The context must be current.
  virtual std::string GetExtensions();

  // Returns whether the current context supports the named extension. The
  // context must be current.
  bool HasExtension(const char* name);

  static bool InitializeOneOff();

#if !defined(OS_MACOSX)
  // Create a GL context that renders directly to a view.
  static GLContext* CreateViewGLContext(gfx::PluginWindowHandle window,
                                        bool multisampled);
#endif

  // Create a GL context used for offscreen rendering. It is initially backed by
  // a 1x1 pbuffer. Use it to create an FBO to do useful rendering.
  // |share_context|, if non-NULL, is a context which the internally created
  // OpenGL context shares textures and other resources.
  static GLContext* CreateOffscreenGLContext(GLContext* shared_context);

 protected:
  bool InitializeCommon();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

}  // namespace gfx

#endif  // APP_GFX_GL_GL_CONTEXT_H_
