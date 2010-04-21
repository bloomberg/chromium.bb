// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ViewGLContext and PbufferGLContext classes.

#include <dlfcn.h>
#include <GL/glew.h>
#include <GL/glxew.h>
#include <GL/glx.h>
#include <GL/osmew.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "app/x11_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "gfx/gl/gl_context.h"
#include "gfx/gl/gl_context_osmesa.h"

namespace gfx {

typedef GLXContext GLContextHandle;
typedef GLXPbuffer PbufferHandle;

// This class is a wrapper around a GL context that renders directly to a
// window.
class ViewGLContext : public GLContext {
 public:
  explicit ViewGLContext(gfx::PluginWindowHandle window)
      : window_(window),
        context_(NULL) {
    DCHECK(window);
  }

  // Initializes the GL context.
  bool Initialize(bool multisampled);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

 private:
  gfx::PluginWindowHandle window_;
  GLContextHandle context_;

  DISALLOW_COPY_AND_ASSIGN(ViewGLContext);
};

// This class is a wrapper around a GL context used for offscreen rendering.
// It is initially backed by a 1x1 pbuffer. Use it to create an FBO to do useful
// rendering.
class PbufferGLContext : public GLContext {
 public:
  explicit PbufferGLContext()
      : context_(NULL),
        pbuffer_(NULL) {
  }

  // Initializes the GL context.
  bool Initialize(void* shared_handle);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

 private:
  GLContextHandle context_;
  PbufferHandle pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLContext);
};

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

static bool InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  osmewInit();
  if (!OSMesaCreateContext) {
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
    // glxewContextInit really only needs a display connection to
    // complete, and we don't want to have to create an OpenGL context
    // just to get access to GLX 1.3 entry points to create pbuffers.
    // We therefore added a glxewContextInitWithDisplay entry point.
    Display* display = x11_util::GetXDisplay();
    if (glxewContextInitWithDisplay(display) != GLEW_OK) {
      LOG(ERROR) << "glxewContextInit failed";
      return false;
    }
  }

  initialized = true;
  return true;
}

bool ViewGLContext::Initialize(bool multisampled) {
  if (multisampled) {
    DLOG(WARNING) << "Multisampling not implemented.";
  }

  Display* display = x11_util::GetXDisplay();
  XWindowAttributes attributes;
  XGetWindowAttributes(display, window_, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);
  int visual_info_count = 0;
  scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info_list(
      XGetVisualInfo(display, VisualIDMask,
                     &visual_info_template,
                     &visual_info_count));
  DCHECK(visual_info_list.get());
  DCHECK_GT(visual_info_count, 0);
  context_ = NULL;
  for (int i = 0; i < visual_info_count; ++i) {
    context_ = glXCreateContext(display, visual_info_list.get() + i, 0, True);
    if (context_)
      break;
  }
  if (!context_) {
    DLOG(ERROR) << "Couldn't create GL context.";
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    DLOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeGLEW()) {
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    Destroy();
    return false;
  }

  return true;
}

void ViewGLContext::Destroy() {
  Display* display = x11_util::GetXDisplay();
  Bool result = glXMakeCurrent(display, 0, 0);

  // glXMakeCurrent isn't supposed to fail when unsetting the context, unless
  // we have pending draws on an invalid window - which shouldn't be the case
  // here.
  DCHECK(result);
  if (context_) {
    glXDestroyContext(display, context_);
    context_ = NULL;
  }
}

bool ViewGLContext::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }

  Display* display = x11_util::GetXDisplay();
  if (glXMakeCurrent(display, window_, context_) != True) {
    glXDestroyContext(display, context_);
    context_ = 0;
    DLOG(ERROR) << "Couldn't make context current.";
    return false;
  }

  return true;
}

bool ViewGLContext::IsCurrent() {
  return glXGetCurrentDrawable() == window_ &&
      glXGetCurrentContext() == context_;
}

bool ViewGLContext::IsOffscreen() {
  return false;
}

void ViewGLContext::SwapBuffers() {
  Display* display = x11_util::GetXDisplay();
  glXSwapBuffers(display, window_);
}

gfx::Size ViewGLContext::GetSize() {
  XWindowAttributes attributes;
  Display* display = x11_util::GetXDisplay();
  XGetWindowAttributes(display, window_, &attributes);
  return gfx::Size(attributes.width, attributes.height);
}

void* ViewGLContext::GetHandle() {
  return context_;
}

GLContext* GLContext::CreateViewGLContext(gfx::PluginWindowHandle window,
                                          bool multisampled) {
  if (!InitializeOneOff())
    return NULL;

  if (OSMesaCreateContext) {
    // TODO(apatrick): Support OSMesa rendering to a window on Linux.
    NOTREACHED() << "OSMesa rendering to a window is not yet implemented.";
    return NULL;
  } else {
    scoped_ptr<ViewGLContext> context(new ViewGLContext(window));

    if (!context->Initialize(multisampled))
      return NULL;

    return context.release();
  }
}

bool PbufferGLContext::Initialize(void* shared_handle) {
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

  Display* display = x11_util::GetXDisplay();

  int nelements = 0;
  // TODO(kbr): figure out whether hardcoding screen to 0 is sufficient.
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> config(
      glXChooseFBConfig(display, 0, config_attributes, &nelements));
  if (!config.get()) {
    DLOG(ERROR) << "glXChooseFBConfig failed.";
    return false;
  }
  if (!nelements) {
    DLOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
    return false;
  }
  context_ = glXCreateNewContext(display,
                                 config.get()[0],
                                 GLX_RGBA_TYPE,
                                 static_cast<GLContextHandle>(shared_handle),
                                 True);
  if (!context_) {
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
  pbuffer_ = glXCreatePbuffer(display,
                              config.get()[0], pbuffer_attributes);
  if (!pbuffer_) {
    Destroy();
    DLOG(ERROR) << "glXCreatePbuffer failed.";
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    DLOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeGLEW()) {
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLContext::Destroy() {
  Display* display = x11_util::GetXDisplay();
  Bool result = glXMakeCurrent(display, 0, 0);
  // glXMakeCurrent isn't supposed to fail when unsetting the context, unless
  // we have pending draws on an invalid window - which shouldn't be the case
  // here.
  DCHECK(result);
  if (context_) {
    glXDestroyContext(display, context_);
    context_ = NULL;
  }

  if (pbuffer_) {
    glXDestroyPbuffer(display, pbuffer_);
    pbuffer_ = NULL;
  }
}

bool PbufferGLContext::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }
  Display* display = x11_util::GetXDisplay();
  if (glXMakeCurrent(display, pbuffer_, context_) != True) {
    glXDestroyContext(display, context_);
    context_ = NULL;
    DLOG(ERROR) << "Couldn't make context current.";
    return false;
  }

  return true;
}

bool PbufferGLContext::IsCurrent() {
  return glXGetCurrentDrawable() == pbuffer_ &&
      glXGetCurrentContext() == context_;
}

bool PbufferGLContext::IsOffscreen() {
  return true;
}

void PbufferGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
}

gfx::Size PbufferGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of this pbuffer.";
  return gfx::Size(1, 1);
}

void* PbufferGLContext::GetHandle() {
  return context_;
}

GLContext* GLContext::CreateOffscreenGLContext(void* shared_handle) {
  if (!InitializeOneOff())
    return NULL;

  if (OSMesaCreateContext) {
    scoped_ptr<OSMesaGLContext> context(new OSMesaGLContext);

    if (!context->Initialize(shared_handle))
      return NULL;

    return context.release();
  } else {
    scoped_ptr<PbufferGLContext> context(new PbufferGLContext);
    if (!context->Initialize(shared_handle))
      return NULL;

    return context.release();
  }
}

}  // namespace gfx
