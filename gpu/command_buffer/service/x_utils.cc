// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class implements the XWindowWrapper class.

#include <dlfcn.h>

#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/precompile.h"
#include "gpu/command_buffer/common/logging.h"
#include "gpu/command_buffer/service/x_utils.h"

namespace gpu {

// scoped_ptr functor for XFree(). Use as follows:
//   scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> foo(...);
// where "XVisualInfo" is any X type that is freed with XFree.
class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

// Some versions of NVIDIA's GL libGL.so include a broken version of
// dlopen/dlsym, and so linking it into chrome breaks it. So we dynamically
// load it, and use glew to dynamically resolve symbols.
// See http://code.google.com/p/chromium/issues/detail?id=16800

static bool g_glxew_initialized = false;
static bool g_glew_initialized = false;

static bool InitializeGLXEW() {
  if (!g_glxew_initialized) {
    void* handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
      LOG(ERROR) << "Could not find libGL.so.1";
      return false;
    }

    // Initializes context-independent parts of GLEW
    if (glxewInit() != GLEW_OK) {
      LOG(ERROR) << "glxewInit failed";
      return false;
    }
    // Hack to work around apparently incorrect assumption in
    // glxewContextInit. Documentation indicates that all of the work
    // it does is actually context-independent. We require GLX 1.3
    // entry points to be present for successful initialization of the
    // off-screen code path in the GLES2Decoder. TODO(kbr): clean this
    // up.
    if (glxewContextInit() != GLEW_OK) {
      LOG(ERROR) << "glxewContextInit failed";
      return false;
    }
    g_glxew_initialized = true;
  }
  return true;
}

// GLEW initialization is extremely expensive because it looks up
// hundreds of function pointers. Realistically we are not going to
// switch between GL implementations on the fly, so for the time being
// we only do the context-dependent GLEW initialization once.
static bool InitializeGLEW() {
  if (!g_glew_initialized) {
    // Initializes context-dependent parts of GLEW
    if (glewInit() != GLEW_OK) {
      LOG(ERROR) << "GLEW failed initialization";
      return false;
    }

    if (!glewIsSupported("GL_VERSION_2_0")) {
      LOG(ERROR) << "GL implementation doesn't support GL version 2.0";
      return false;
    }
    g_glew_initialized = true;
  }
  return true;
}

GLXContextWrapper::~GLXContextWrapper() {
}

bool GLXContextWrapper::Initialize() {
  if (!MakeCurrent()) {
    Destroy();
    DLOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeGLEW()) {
    Destroy();
    return false;
  }

  return true;
}

void GLXContextWrapper::Destroy() {
  Bool result = glXMakeCurrent(GetDisplay(), 0, 0);
  // glXMakeCurrent isn't supposed to fail when unsetting the context, unless
  // we have pending draws on an invalid window - which shouldn't be the case
  // here.
  DCHECK(result);
  if (GetContext()) {
    glXDestroyContext(GetDisplay(), GetContext());
    SetContext(NULL);
  }
}

bool XWindowWrapper::Initialize() {
  if (!InitializeGLXEW())
    return false;

  XWindowAttributes attributes;
  XGetWindowAttributes(GetDisplay(), window_, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info_list(
      XGetVisualInfo(GetDisplay(), VisualIDMask,
                     &visual_info_template,
                     &visual_info_count));
  DCHECK(visual_info_list.get());
  DCHECK_GT(visual_info_count, 0);
  SetContext(NULL);
  for (int i = 0; i < visual_info_count; ++i) {
    SetContext(glXCreateContext(GetDisplay(), visual_info_list.get() + i, 0,
                                True));
    if (GetContext())
      break;
  }
  if (!GetContext()) {
    DLOG(ERROR) << "Couldn't create GL context.";
    return false;
  }
  return GLXContextWrapper::Initialize();
}

bool XWindowWrapper::MakeCurrent() {
  if (glXGetCurrentDrawable() == window_ &&
      glXGetCurrentContext() == GetContext()) {
    return true;
  }
  if (glXMakeCurrent(GetDisplay(), window_, GetContext()) != True) {
    glXDestroyContext(GetDisplay(), GetContext());
    SetContext(0);
    DLOG(ERROR) << "Couldn't make context current.";
    return false;
  }
  return true;
}

bool XWindowWrapper::IsOffscreen() {
  return false;
}

void XWindowWrapper::SwapBuffers() {
  glXSwapBuffers(GetDisplay(), window_);
}

bool GLXPbufferWrapper::Initialize() {
  if (!InitializeGLXEW())
    return false;

  if (!glXChooseFBConfig ||
      !glXCreateNewContext ||
      !glXCreatePbuffer ||
      !glXDestroyPbuffer) {
    DLOG(ERROR) << "Pbuffer support not available.";
    return false;
  }

  static const int config_attributes[] = {
    GLX_DRAWABLE_TYPE,
    GLX_PBUFFER_BIT,
    GLX_RENDER_TYPE,
    GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,
    0,
    0
  };

  int nelements = 0;
  // TODO(kbr): figure out whether hardcoding screen to 0 is sufficient.
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> config(
      glXChooseFBConfig(GetDisplay(), 0, config_attributes, &nelements));
  if (!config.get()) {
    DLOG(ERROR) << "glXChooseFBConfig failed.";
    return false;
  }
  if (!nelements) {
    DLOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
    return false;
  }
  SetContext(glXCreateNewContext(GetDisplay(),
                                 config.get()[0], GLX_RGBA_TYPE, 0, True));
  if (!GetContext()) {
    DLOG(ERROR) << "glXCreateNewContext failed.";
    return false;
  }
  static const int pbuffer_attributes[] = {
    GLX_PBUFFER_WIDTH,
    1,
    GLX_PBUFFER_HEIGHT,
    1,
    0
  };
  pbuffer_ = glXCreatePbuffer(GetDisplay(),
                              config.get()[0], pbuffer_attributes);
  if (!pbuffer_) {
    Destroy();
    DLOG(ERROR) << "glXCreatePbuffer failed.";
    return false;
  }
  return GLXContextWrapper::Initialize();
}

void GLXPbufferWrapper::Destroy() {
  GLXContextWrapper::Destroy();
  if (pbuffer_) {
    glXDestroyPbuffer(GetDisplay(), pbuffer_);
    pbuffer_ = NULL;
  }
}

bool GLXPbufferWrapper::MakeCurrent() {
  if (glXGetCurrentDrawable() == pbuffer_ &&
      glXGetCurrentContext() == GetContext()) {
    return true;
  }
  if (glXMakeCurrent(GetDisplay(), pbuffer_, GetContext()) != True) {
    glXDestroyContext(GetDisplay(), GetContext());
    SetContext(0);
    DLOG(ERROR) << "Couldn't make context current.";
    return false;
  }
  return true;
}

bool GLXPbufferWrapper::IsOffscreen() {
  return true;
}

void GLXPbufferWrapper::SwapBuffers() {
}

}  // namespace gpu
