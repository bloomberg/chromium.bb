// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>

#include "build/build_config.h"
#if defined(OS_LINUX)
#include "app/x11_util.h"
#define EGL_HAS_PBUFFERS 1
#endif
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_egl.h"

namespace gfx {

namespace {

// The EGL configuration to use.
EGLDisplay g_display;
EGLConfig g_config;

}  // namespace anonymous

bool BaseEGLContext::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

#ifdef OS_LINUX
  EGLNativeDisplayType native_display = x11_util::GetXDisplay();
#else
  EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;
#endif
  g_display = eglGetDisplay(native_display);
  if (!g_display) {
    LOG(ERROR) << "eglGetDisplay failed.";
    return false;
  }

  if (!eglInitialize(g_display, NULL, NULL)) {
    LOG(ERROR) << "eglInitialize failed.";
    return false;
  }

  // Choose an EGL configuration.
  static const EGLint kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_DEPTH_SIZE, 16,
    EGL_STENCIL_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
#ifdef EGL_HAS_PBUFFERS
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
#else
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#endif
    EGL_NONE
  };

  EGLint num_configs;
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       NULL,
                       0,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed.";
    return false;
  }

  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return false;
  }

  scoped_array<EGLConfig> configs(new EGLConfig[num_configs]);
  if (!eglChooseConfig(g_display,
                       kConfigAttribs,
                       configs.get(),
                       num_configs,
                       &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed.";
    return false;
  }

  g_config = configs[0];

  initialized = true;
  return true;
}

EGLDisplay BaseEGLContext::GetDisplay() {
  return g_display;
}

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

  // Create a surface for the native window.
  EGLNativeWindowType native_window =
      reinterpret_cast<EGLNativeWindowType>(window_);
  surface_ = eglCreateWindowSurface(g_display, g_config, native_window, NULL);

  if (!surface_) {
    LOG(ERROR) << "eglCreateWindowSurface failed.";
    Destroy();
    return false;
  }

  // Create a context.
  context_ = eglCreateContext(g_display, g_config, NULL, NULL);
  if (!context_) {
    LOG(ERROR) << "eglCreateContext failed.";
    Destroy();
    return false;
  }

  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
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

bool NativeViewEGLContext::SwapBuffers() {
  return eglSwapBuffers(g_display, surface_) == EGL_TRUE;
}

gfx::Size NativeViewEGLContext::GetSize() {
#if defined(OS_WIN)
  RECT rect;
  CHECK(GetClientRect(static_cast<HWND>(window_), &rect));
  return gfx::Size(rect.right - rect.left, rect.bottom - rect.top);
#else
  // TODO(piman): This doesn't work correctly on Windows yet, the size doesn't
  // get updated on resize. When it does, we can share the code.
  EGLint width;
  EGLint height;
  CHECK(eglQuerySurface(g_display, surface_, EGL_WIDTH, &width));
  CHECK(eglQuerySurface(g_display, surface_, EGL_HEIGHT, &height));
  return gfx::Size(width, height);
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
      own_surface_(false),
      context_(NULL)
{
}

SecondaryEGLContext::~SecondaryEGLContext() {
}

bool SecondaryEGLContext::Initialize(GLContext* shared_context) {
  DCHECK(!context_);

  static const EGLint kContextAttributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  if (shared_context) {
    surface_ = static_cast<BaseEGLContext*>(shared_context)->GetSurface();
    own_surface_ = false;

    // Create a context.
    context_ = eglCreateContext(g_display,
                                g_config,
                                shared_context->GetHandle(),
                                kContextAttributes);
  } else {
#ifdef EGL_HAS_PBUFFERS
    static const EGLint kPbufferAttribs[] = {
      EGL_WIDTH, 1,
      EGL_HEIGHT, 1,
      EGL_NONE
    };

    surface_ = eglCreatePbufferSurface(g_display, g_config, kPbufferAttribs);
    if (!surface_) {
      EGLint error = eglGetError();
      LOG(ERROR) << "Error creating Pbuffer: " << error;
      return false;
    }
    own_surface_ = true;

    context_ = eglCreateContext(g_display, g_config, NULL, kContextAttributes);
#else
    NOTIMPLEMENTED() << "Offscreen non-shared GLES context";
    return false;
#endif
  }

  if (!context_) {
    EGLint error = eglGetError();
    LOG(ERROR) << "Error creating context: " << error;
    Destroy();
    return false;
  }

  return true;
}

void SecondaryEGLContext::Destroy() {
  if (own_surface_) {
    eglDestroySurface(g_display, surface_);
    own_surface_ = false;
  }
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

bool SecondaryEGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a SecondaryEGLContext.";
  return false;
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
