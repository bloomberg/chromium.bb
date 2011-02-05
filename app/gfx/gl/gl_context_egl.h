// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GL_GL_CONTEXT_EGL_H_
#define APP_GFX_GL_GL_CONTEXT_EGL_H_
#pragma once

#include "app/gfx/gl/gl_context.h"
#include "base/ref_counted.h"
#include "ui/gfx/size.h"

typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLSurface;

namespace gfx {

// Takes ownership of an EGL surface and reference counts it so it can be shared
// by multiple EGL contexts and destroyed with the last.
class SharedEGLSurface : public base::RefCounted<SharedEGLSurface> {
 public:
  explicit SharedEGLSurface(EGLSurface surface);
  ~SharedEGLSurface();

  EGLSurface egl_surface() const;

 private:
  EGLSurface surface_;
  DISALLOW_COPY_AND_ASSIGN(SharedEGLSurface);
};

// Interface for EGL contexts. Adds an EGL specific accessor for retreiving
// the surface.
class BaseEGLContext : public GLContext {
 public:
  BaseEGLContext() {}
  virtual ~BaseEGLContext() {}

  static bool InitializeOneOff();

  static EGLDisplay GetDisplay();

  // Get the associated EGL surface.
  virtual SharedEGLSurface* GetSurface() = 0;

  // Implement GLContext.
  virtual std::string GetExtensions();

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseEGLContext);
};

// Encapsulates an EGL OpenGL ES context that renders to a view.
class NativeViewEGLContext : public BaseEGLContext {
 public:
  explicit NativeViewEGLContext(void* window);
  virtual ~NativeViewEGLContext();

  // Initialize an EGL context.
  bool Initialize();

  // Implement GLContext.
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

  // Implement BaseEGLContext.
  virtual SharedEGLSurface* GetSurface();

 private:
  void* window_;
  scoped_refptr<SharedEGLSurface> surface_;
  EGLContext context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewEGLContext);
};

// Encapsulates an EGL OpenGL ES context intended for offscreen use. It is
// actually associated with a native window or a pbuffer on supporting platforms
// and will render to it. The caller must bind an FBO to prevent this.
// TODO(apatrick): implement pbuffers in ANGLE and change this to
// PbufferEGLContext and use it on all EGL platforms.
class SecondaryEGLContext : public BaseEGLContext {
 public:
  SecondaryEGLContext();
  virtual ~SecondaryEGLContext();

  // Initialize an EGL context that shares a namespace with another.
  bool Initialize(GLContext* shared_context);

  // Implement GLContext.
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

  // Implement BaseEGLContext.
  virtual SharedEGLSurface* GetSurface();

 private:
  scoped_refptr<SharedEGLSurface> surface_;
  EGLContext context_;

  DISALLOW_COPY_AND_ASSIGN(SecondaryEGLContext);
};

}  // namespace gfx

#endif  // APP_GFX_GL_GL_CONTEXT_EGL_H_
