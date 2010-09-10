// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GL_GL_CONTEXT_EGL_H_
#define APP_GFX_GL_GL_CONTEXT_EGL_H_
#pragma once

#include "gfx/size.h"
#include "app/gfx/gl/gl_context.h"

typedef void* EGLDisplay;
typedef void* EGLContext;
typedef void* EGLSurface;

namespace gfx {

// Interface for EGL contexts. Adds an EGL specific accessor for retreiving
// the surface.
class BaseEGLContext : public GLContext {
 public:
  BaseEGLContext() {}
  virtual ~BaseEGLContext() {}

  // Implement GLContext.
  virtual EGLSurface GetSurface() = 0;

  static bool InitializeOneOff();

  static EGLDisplay GetDisplay();

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

  // Implement BaseEGLContext.
  virtual EGLSurface GetSurface();

 private:
  void* window_;
  EGLSurface surface_;
  EGLContext context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewEGLContext);
};

// Encapsulates an EGL OpenGL ES context intended for offscreen use. It is
// actually associated with a native window and will render to it. The caller
// must bind an FBO to prevent this. Not using pbuffers because ANGLE does not
// support them.
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

  // Implement BaseEGLContext.
  virtual EGLSurface GetSurface();

 private:
  // All offscreen
  EGLSurface surface_;
  bool own_surface_;
  EGLContext context_;

  DISALLOW_COPY_AND_ASSIGN(SecondaryEGLContext);
};

}  // namespace gfx

#endif  // APP_GFX_GL_GL_CONTEXT_EGL_H_
