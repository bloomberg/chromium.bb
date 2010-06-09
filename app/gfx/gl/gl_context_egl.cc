// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>

#include "base/scoped_ptr.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_egl.h"

namespace gfx {

namespace {

// The EGL configuration to use.
EGLDisplay g_display;
EGLConfig g_config;

bool InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!g_display)
    return false;

  if (!eglInitialize(g_display, NULL, NULL) == EGL_TRUE)
    return false;

  // Choose an EGL configuration.
  static const EGLint kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  EGLint num_configs;
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       NULL,
                       0,
                       &num_configs)) {
    return false;
  }

  if (num_configs == 0)
    return false;

  scoped_array<EGLConfig> configs(new EGLConfig[num_configs]);
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       configs.get(),
                       num_configs,
                       &num_configs)) {
    return false;
  }

  g_config = configs[0];

  initialized = true;
  return true;
}
}  // namespace anonymous

NativeViewEGLContext::NativeViewEGLContext(void* window)
    : window_(window),
      surface_(NULL),
      context_(NULL)
{
}

NativeViewEGLContext::~NativeViewEGLContext() {
}

bool NativeViewEGLContext::Initialize() {
  DCHECK(!context_);

  if (!InitializeOneOff())
    return NULL;

  // Create a surface for the native window.
  surface_ = eglCreateWindowSurface(g_display,
                                    g_config,
                                    static_cast<EGLNativeWindowType>(window_),
                                    NULL);
  if (!surface_) {
    Destroy();
    return false;
  }

  // Create a context.
  context_ = eglCreateContext(g_display, g_config, NULL, NULL);
  if (!context_) {
    Destroy();
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    Destroy();
    return false;
  }

  return true;
}

void NativeViewEGLContext::Destroy() {
  if (context_) {
    eglDestroyContext(g_display, context_);
    context_ = NULL;
  }

   if (surface_) {
     eglDestroySurface(g_display, surface_);
     surface_ = NULL;
   }
}

bool NativeViewEGLContext::MakeCurrent() {
  DCHECK(context_);
  return eglMakeCurrent(g_display,
                        surface_, surface_,
                        context_) == GL_TRUE;
}

bool NativeViewEGLContext::IsCurrent() {
  DCHECK(context_);
  return context_ == eglGetCurrentContext();
}

bool NativeViewEGLContext::IsOffscreen() {
  return false;
}

void NativeViewEGLContext::SwapBuffers() {
  eglSwapBuffers(g_display, surface_);
}

gfx::Size NativeViewEGLContext::GetSize() {
#if defined(OS_WIN)
  RECT rect;
  CHECK(GetClientRect(static_cast<HWND>(window_), &rect));
  return gfx::Size(rect.right - rect.left, rect.bottom - rect.top);
#else
  NOTREACHED()
      << "NativeViewEGLContext::GetSize not implemented on this platform.";
#endif
}

void* NativeViewEGLContext::GetHandle() {
  return context_;
}

EGLSurface NativeViewEGLContext::GetSurface() {
  return surface_;
}

SecondaryEGLContext::SecondaryEGLContext()
    : surface_(NULL),
      context_(NULL)
{
}

SecondaryEGLContext::~SecondaryEGLContext() {
}

bool SecondaryEGLContext::Initialize(GLContext* shared_context) {
  DCHECK(shared_context);
  DCHECK(!context_);

  if (!InitializeOneOff())
    return NULL;

  surface_ = static_cast<BaseEGLContext*>(shared_context)->GetSurface();

  // Create a context.
  context_ = eglCreateContext(g_display,
                              g_config,
                              shared_context->GetHandle(),
                              NULL);
  if (!context_) {
    Destroy();
    return false;
  }

  return true;
}

void SecondaryEGLContext::Destroy() {
  surface_ = NULL;

  if (context_) {
    eglDestroyContext(g_display, context_);
    context_ = NULL;
  }
}

bool SecondaryEGLContext::MakeCurrent() {
  DCHECK(context_);
  return eglMakeCurrent(g_display,
                        surface_, surface_,
                        context_) == GL_TRUE;
}

bool SecondaryEGLContext::IsCurrent() {
  DCHECK(context_);
  return context_ == eglGetCurrentContext();
}

bool SecondaryEGLContext::IsOffscreen() {
  return true;
}

void SecondaryEGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a SecondaryEGLContext.";
}

gfx::Size SecondaryEGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of this SecondaryEGLContext.";
  return gfx::Size(1, 1);
}

void* SecondaryEGLContext::GetHandle() {
  return context_;
}

EGLSurface SecondaryEGLContext::GetSurface() {
  return surface_;
}

}  // namespace gfx
