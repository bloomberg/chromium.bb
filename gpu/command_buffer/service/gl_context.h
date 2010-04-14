// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_H_

#include <build/build_config.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"
#include "gpu/command_buffer/common/logging.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {

#if defined(UNIT_TEST)
typedef void* HDC;
struct Display;
typedef void* GLContextHandle;
typedef void* PbufferHandle;
#elif defined(OS_WIN)
typedef HGLRC GLContextHandle;
typedef HPBUFFERARB PbufferHandle;
#elif defined(OS_LINUX)
typedef GLXContext GLContextHandle;
typedef GLXPbuffer PbufferHandle;
#elif defined(OS_MACOSX)
typedef CGLContextObj GLContextHandle;
typedef CGLPBufferObj PbufferHandle;
#endif

bool InitializeGLEW();

// Encapsulates an OpenGL context, hiding platform specific management.
class GLContext {
 public:
  GLContext();
  virtual ~GLContext();

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
  virtual void SwapBuffers() = 0;

  // Get the size of the back buffer.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific GL context "handle".
  virtual GLContextHandle GetHandle() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

// This class is a wrapper around a GL context that renders directly to a
// window.
class ViewGLContext : public GLContext {
 public:
#if defined(OS_WIN)
  explicit ViewGLContext(gfx::PluginWindowHandle window)
      : window_(window),
        device_context_(NULL),
        context_(NULL) {
    DCHECK(window);
  }
#elif defined(OS_LINUX)
  ViewGLContext(Display* display, gfx::PluginWindowHandle window)
      : display_(display),
        window_(window),
        context_(NULL) {
    DCHECK(display);
    DCHECK(window);
  }
#elif defined(OS_MACOSX)
  ViewGLContext() {
    NOTIMPLEMENTED() << "ViewGLContext not supported on Mac platform.";
  }
#endif

  // Initializes the GL context.
  bool Initialize(bool multisampled);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual GLContextHandle GetHandle();

 private:
#if defined(OS_WIN)
  gfx::PluginWindowHandle window_;
  HDC device_context_;
  GLContextHandle context_;
#elif defined(OS_LINUX)
  Display* display_;
  gfx::PluginWindowHandle window_;
  GLContextHandle context_;
#elif defined(OS_MACOSX)
  // This context isn't implemented on Mac OS X.
#endif

  DISALLOW_COPY_AND_ASSIGN(ViewGLContext);
};

// This class is a wrapper around a GL context used for offscreen rendering.
// It is initially backed by a 1x1 pbuffer. Use it to create an FBO to do useful
// rendering.
class PbufferGLContext : public GLContext {
 public:
#if defined(OS_WIN)
  PbufferGLContext()
      : context_(NULL),
        device_context_(NULL),
        pbuffer_(NULL) {
  }
#elif defined(OS_LINUX)
  explicit PbufferGLContext(Display* display)
      : context_(NULL),
        display_(display),
        pbuffer_(NULL) {
    DCHECK(display_);
  }
#elif defined(OS_MACOSX)
  PbufferGLContext()
      : context_(NULL),
        pbuffer_(NULL) {
  }
#endif

  // Initializes the GL context.
  bool Initialize(GLContext* shared_context);
  bool Initialize(GLContextHandle shared_handle);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual GLContextHandle GetHandle();

 private:
  GLContextHandle context_;
#if defined(OS_WIN)
  HDC device_context_;
  PbufferHandle pbuffer_;
#elif defined(OS_LINUX)
  Display* display_;
  PbufferHandle pbuffer_;
#elif defined(OS_MACOSX)
  PbufferHandle pbuffer_;
#endif
  DISALLOW_COPY_AND_ASSIGN(PbufferGLContext);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_H_
