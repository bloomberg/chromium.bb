// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_context_egl.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "third_party/angle/include/EGL/egl.h"

// This header must come after the above third-party include, as
// it brings in #defines that cause conflicts.
#include "app/gfx/gl/gl_bindings.h"

#if defined(OS_LINUX)
extern "C" {
#include <X11/Xlib.h>
}
#define EGL_HAS_PBUFFERS 1
#endif

namespace gfx {

namespace {

// The EGL configuration to use.
EGLDisplay g_display;
EGLConfig g_config;

// Returns the last EGL error as a string.
const char* GetLastEGLErrorString() {
  EGLint error = eglGetError();
  switch (error) {
    case EGL_SUCCESS:
      return "EGL_SUCCESS";
    case EGL_BAD_ACCESS:
      return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONTEXT:
      return "EGL_BAD_CONTEXT";
    case EGL_BAD_CONFIG:
      return "EGL_BAD_CONFIG";
    case EGL_BAD_CURRENT_SURFACE:
      return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "EGL_BAD_DISPLAY";
    case EGL_BAD_SURFACE:
      return "EGL_BAD_SURFACE";
    case EGL_BAD_MATCH:
      return "EGL_BAD_MATCH";
    case EGL_BAD_PARAMETER:
      return "EGL_BAD_PARAMETER";
    case EGL_BAD_NATIVE_PIXMAP:
      return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "EGL_BAD_NATIVE_WINDOW";
    default:
      return "UNKNOWN";
  }
}
}  // namespace anonymous

SharedEGLSurface::SharedEGLSurface(EGLSurface surface) : surface_(surface) {
}

SharedEGLSurface::~SharedEGLSurface() {
  if (surface_) {
    if (!eglDestroySurface(g_display, surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << GetLastEGLErrorString();
    }
  }
}

EGLSurface SharedEGLSurface::egl_surface() const {
  return surface_;
}

bool BaseEGLContext::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

#ifdef OS_LINUX
  EGLNativeDisplayType native_display = XOpenDisplay(NULL);
#else
  EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;
#endif
  g_display = eglGetDisplay(native_display);
  if (!g_display) {
    LOG(ERROR) << "eglGetDisplay failed with error " << GetLastEGLErrorString();
    return false;
  }

  if (!eglInitialize(g_display, NULL, NULL)) {
    LOG(ERROR) << "eglInitialize failed with error " << GetLastEGLErrorString();
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
    LOG(ERROR) << "eglChooseConfig failed failed with error "
               << GetLastEGLErrorString();
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
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return false;
  }

  g_config = configs[0];

  initialized = true;
  return true;
}

EGLDisplay BaseEGLContext::GetDisplay() {
  return g_display;
}

std::string BaseEGLContext::GetExtensions() {
  const char* extensions = eglQueryString(g_display, EGL_EXTENSIONS);
  if (!extensions)
    return GLContext::GetExtensions();

  return GLContext::GetExtensions() + " " + extensions;
}

NativeViewEGLContext::NativeViewEGLContext(void* window)
    : window_(window),
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
  surface_ = new SharedEGLSurface(eglCreateWindowSurface(g_display,
                                                         g_config,
                                                         native_window,
                                                         NULL));

  if (!surface_->egl_surface()) {
    LOG(ERROR) << "eglCreateWindowSurface failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  static const EGLint kContextAttributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  context_ = eglCreateContext(g_display, g_config, NULL, kContextAttributes);
  if (!context_) {
    LOG(ERROR) << "eglCreateContext failed with error "
               << GetLastEGLErrorString();
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
    if (!eglDestroyContext(g_display, context_)) {
      LOG(ERROR) << "eglDestroyContext failed with error "
                 << GetLastEGLErrorString();
    }

    context_ = NULL;
  }

  surface_ = NULL;
}

bool NativeViewEGLContext::MakeCurrent() {
  DCHECK(context_);
  if (!eglMakeCurrent(g_display,
                      surface_->egl_surface(),
                      surface_->egl_surface(),
                      context_)) {
    VLOG(1) << "eglMakeCurrent failed with error "
            << GetLastEGLErrorString();
    return false;
  }

  return true;
}

bool NativeViewEGLContext::IsCurrent() {
  DCHECK(context_);
  return context_ == eglGetCurrentContext();
}

bool NativeViewEGLContext::IsOffscreen() {
  return false;
}

bool NativeViewEGLContext::SwapBuffers() {
  if (!eglSwapBuffers(g_display, surface_->egl_surface())) {
    VLOG(1) << "eglSwapBuffers failed with error "
            << GetLastEGLErrorString();
    return false;
  }

  return true;
}

gfx::Size NativeViewEGLContext::GetSize() {
#if defined(OS_WIN)
  RECT rect;
  if (!GetClientRect(static_cast<HWND>(window_), &rect)) {
    DCHECK(false) << "GetClientRect failed.";
    return gfx::Size();
  }

  return gfx::Size(rect.right - rect.left, rect.bottom - rect.top);
#else
  // TODO(piman): This doesn't work correctly on Windows yet, the size doesn't
  // get updated on resize. When it does, we can share the code.
  EGLint width;
  EGLint height;
  if (!eglQuerySurface(
          g_display, surface_->egl_surface(), EGL_WIDTH, &width) ||
      !eglQuerySurface(
          g_display, surface_->egl_surface(), EGL_HEIGHT, &height)) {
    NOTREACHED() << "eglQuerySurface failed with error "
                 << GetLastEGLErrorString();
    return gfx::Size();
  }

  return gfx::Size(width, height);
#endif
}

void* NativeViewEGLContext::GetHandle() {
  return context_;
}

void NativeViewEGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  if (!eglSwapInterval(g_display, interval)) {
    LOG(ERROR) << "eglSwapInterval failed with error "
               << GetLastEGLErrorString();
  }
}

SharedEGLSurface* NativeViewEGLContext::GetSurface() {
  return surface_;
}

SecondaryEGLContext::SecondaryEGLContext()
    : context_(NULL)
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

    surface_ = new SharedEGLSurface(eglCreatePbufferSurface(g_display,
                                                            g_config,
                                                            kPbufferAttribs));
    if (!surface_->egl_surface()) {
      LOG(ERROR) << "eglCreatePbufferSurface failed with error "
                 << GetLastEGLErrorString();
      Destroy();
      return false;
    }

    context_ = eglCreateContext(g_display, g_config, NULL, kContextAttributes);
#else
    NOTIMPLEMENTED() << "Offscreen non-shared GLES context";
    return false;
#endif
  }

  if (!context_) {
    LOG(ERROR) << "eglCreateContext failed with error "
               << GetLastEGLErrorString();
    Destroy();
    return false;
  }

  return true;
}

void SecondaryEGLContext::Destroy() {
  if (context_) {
    if (!eglDestroyContext(g_display, context_)) {
      LOG(ERROR) << "eglDestroyContext failed with error "
                 << GetLastEGLErrorString();
    }

    context_ = NULL;
  }

  surface_ = NULL;
}

bool SecondaryEGLContext::MakeCurrent() {
  DCHECK(context_);
  if (!eglMakeCurrent(g_display,
                      surface_->egl_surface(),
                      surface_->egl_surface(),
                      context_)) {
    VLOG(1) << "eglMakeCurrent failed with error "
            << GetLastEGLErrorString();
    return false;
  }

  return true;
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

void SecondaryEGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  NOTREACHED() << "Attempt to call SetSwapInterval on a SecondaryEGLContext.";
}

SharedEGLSurface* SecondaryEGLContext::GetSurface() {
  return surface_;
}

}  // namespace gfx
