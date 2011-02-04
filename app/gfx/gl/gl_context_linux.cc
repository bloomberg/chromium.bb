// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ViewGLContext and PbufferGLContext classes.

#include "app/gfx/gl/gl_context.h"

extern "C" {
#include <X11/Xlib.h>
}

#include <GL/osmesa.h>

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_egl.h"
#include "app/gfx/gl/gl_context_osmesa.h"
#include "app/gfx/gl/gl_context_stub.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "ui/base/x/x11_util.h"

namespace {

Display* GetXDisplayHelper() {
  static Display* display = NULL;

  if (!display) {
    display = XOpenDisplay(NULL);
    CHECK(display);
  }

  return display;
}

}  // namespace

namespace gfx {

typedef GLXContext GLContextHandle;
typedef GLXPbuffer PbufferHandle;

class BaseLinuxGLContext : public GLContext {
 public:
  virtual std::string GetExtensions();
};

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
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

 private:
  gfx::PluginWindowHandle window_;
  GLContextHandle context_;

  DISALLOW_COPY_AND_ASSIGN(ViewGLContext);
};

// This class is a wrapper around a GL context that uses OSMesa to render
// to an offscreen buffer and then blits it to a window.
class OSMesaViewGLContext : public GLContext {
 public:
  explicit OSMesaViewGLContext(gfx::PluginWindowHandle window)
      : window_graphics_context_(0),
        window_(window),
        pixmap_graphics_context_(0),
        pixmap_(0) {
    DCHECK(window);
  }

  // Initializes the GL context.
  bool Initialize();

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

 private:
  bool UpdateSize();

  GC window_graphics_context_;
  gfx::PluginWindowHandle window_;
  GC pixmap_graphics_context_;
  Pixmap pixmap_;
  OSMesaGLContext osmesa_context_;

  DISALLOW_COPY_AND_ASSIGN(OSMesaViewGLContext);
};

// This class is a wrapper around a GL context used for offscreen rendering.
// It is initially backed by a 1x1 pbuffer. Use it to create an FBO to do useful
// rendering.
class PbufferGLContext : public GLContext {
 public:
  explicit PbufferGLContext()
      : context_(NULL),
        pbuffer_(0) {
  }

  // Initializes the GL context.
  bool Initialize(GLContext* shared_context);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

 private:
  GLContextHandle context_;
  PbufferHandle pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLContext);
};

// Backup context if Pbuffers (GLX 1.3) aren't supported. May run slower...
class PixmapGLContext : public GLContext {
 public:
  explicit PixmapGLContext()
      : context_(NULL),
        pixmap_(0),
        glx_pixmap_(0) {
  }

  // Initializes the GL context.
  bool Initialize(GLContext* shared_context);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

 private:
  GLContextHandle context_;
  Pixmap pixmap_;
  GLXPixmap glx_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(PixmapGLContext);
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

bool GLContext::InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  static const GLImplementation kAllowedGLImplementations[] = {
    kGLImplementationDesktopGL,
    kGLImplementationEGLGLES2,
    kGLImplementationOSMesaGL
  };

  if (!InitializeRequestedGLBindings(
           kAllowedGLImplementations,
           kAllowedGLImplementations + arraysize(kAllowedGLImplementations),
           kGLImplementationDesktopGL)) {
    LOG(ERROR) << "InitializeRequestedGLBindings failed.";
    return false;
  }

  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      // Only check the GLX version if we are in fact using GLX. We might
      // actually be using the mock GL implementation.
      Display* display = GetXDisplayHelper();
      int major, minor;
      if (!glXQueryVersion(display, &major, &minor)) {
        LOG(ERROR) << "glxQueryVersion failed";
        return false;
      }

      if (major == 1 && minor < 3) {
        LOG(WARNING) << "GLX 1.3 or later is recommended.";
      }

      break;
    }
    case kGLImplementationEGLGLES2:
      if (!BaseEGLContext::InitializeOneOff()) {
        LOG(ERROR) << "BaseEGLContext::InitializeOneOff failed.";
        return false;
      }
      break;
    default:
      break;
  }

  initialized = true;
  return true;
}

std::string BaseLinuxGLContext::GetExtensions() {
  Display* display = GetXDisplayHelper();
  const char* extensions = glXQueryExtensionsString(display, 0);
  if (extensions) {
    return GLContext::GetExtensions() + " " + extensions;
  }

  return GLContext::GetExtensions();
}

bool ViewGLContext::Initialize(bool multisampled) {
  if (multisampled) {
    LOG(WARNING) << "Multisampling not implemented.";
  }

  Display* display = GetXDisplayHelper();
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
    LOG(ERROR) << "Couldn't create GL context.";
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    LOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitlializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void ViewGLContext::Destroy() {
  Display* display = GetXDisplayHelper();
  bool result = glXMakeCurrent(display, 0, 0);

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

  Display* display = GetXDisplayHelper();
  if (glXMakeCurrent(display, window_, context_) != True) {
    glXDestroyContext(display, context_);
    context_ = 0;
    LOG(ERROR) << "Couldn't make context current.";
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

bool ViewGLContext::SwapBuffers() {
  Display* display = GetXDisplayHelper();
  glXSwapBuffers(display, window_);
  return true;
}

gfx::Size ViewGLContext::GetSize() {
  XWindowAttributes attributes;
  Display* display = GetXDisplayHelper();
  XGetWindowAttributes(display, window_, &attributes);
  return gfx::Size(attributes.width, attributes.height);
}

void* ViewGLContext::GetHandle() {
  return context_;
}

void ViewGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  if (HasExtension("GLX_EXT_swap_control") && glXSwapIntervalEXT) {
    Display* display = GetXDisplayHelper();
    glXSwapIntervalEXT(display, window_, interval);
  }
}

bool OSMesaViewGLContext::Initialize() {
  if (!osmesa_context_.Initialize(OSMESA_BGRA, NULL)) {
    LOG(ERROR) << "OSMesaGLContext::Initialize failed.";
    Destroy();
    return false;
  }

  window_graphics_context_ = XCreateGC(GetXDisplayHelper(),
                                       window_,
                                       0,
                                       NULL);
  if (!window_graphics_context_) {
    LOG(ERROR) << "XCreateGC failed.";
    Destroy();
    return false;
  }

  UpdateSize();

  return true;
}

void OSMesaViewGLContext::Destroy() {
  osmesa_context_.Destroy();

  Display* display = GetXDisplayHelper();

  if (pixmap_graphics_context_) {
    XFreeGC(display, pixmap_graphics_context_);
    pixmap_graphics_context_ = NULL;
  }

  if (pixmap_) {
    XFreePixmap(display, pixmap_);
    pixmap_ = 0;
  }

  if (window_graphics_context_) {
    XFreeGC(display, window_graphics_context_);
    window_graphics_context_ = NULL;
  }
}

bool OSMesaViewGLContext::MakeCurrent() {
  // TODO(apatrick): This is a bit of a hack. The window might have had zero
  // size when the context was initialized. Assume it has a valid size when
  // MakeCurrent is called and resize the back buffer if necessary.
  UpdateSize();
  return osmesa_context_.MakeCurrent();
}

bool OSMesaViewGLContext::IsCurrent() {
  return osmesa_context_.IsCurrent();
}

bool OSMesaViewGLContext::IsOffscreen() {
  return false;
}

bool OSMesaViewGLContext::SwapBuffers() {
  // Update the size before blitting so that the blit size is exactly the same
  // as the window.
  if (!UpdateSize()) {
    LOG(ERROR) << "Failed to update size of OSMesaGLContext.";
    return false;
  }

  gfx::Size size = osmesa_context_.GetSize();

  Display* display = GetXDisplayHelper();

  // Copy the frame into the pixmap.
  XWindowAttributes attributes;
  XGetWindowAttributes(display, window_, &attributes);
  ui::PutARGBImage(display,
                   attributes.visual,
                   attributes.depth,
                   pixmap_,
                   pixmap_graphics_context_,
                   static_cast<const uint8*>(osmesa_context_.buffer()),
                   size.width(),
                   size.height());

  // Copy the pixmap to the window.
  XCopyArea(display,
            pixmap_,
            window_,
            window_graphics_context_,
            0, 0,
            size.width(), size.height(),
            0, 0);

  return true;
}

gfx::Size OSMesaViewGLContext::GetSize() {
  return osmesa_context_.GetSize();
}

void* OSMesaViewGLContext::GetHandle() {
  return osmesa_context_.GetHandle();
}

void OSMesaViewGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  // Fail silently. It is legitimate to set the swap interval on a view context
  // but XLib does not have those semantics.
}

bool OSMesaViewGLContext::UpdateSize() {
  // Get the window size.
  XWindowAttributes attributes;
  Display* display = GetXDisplayHelper();
  XGetWindowAttributes(display, window_, &attributes);
  gfx::Size window_size = gfx::Size(std::max(1, attributes.width),
                                    std::max(1, attributes.height));

  // Early out if the size has not changed.
  gfx::Size osmesa_size = osmesa_context_.GetSize();
  if (pixmap_graphics_context_ && pixmap_ && window_size == osmesa_size)
    return true;

  // Change osmesa surface size to that of window.
  osmesa_context_.Resize(window_size);

  // Destroy the previous pixmap and graphics context.
  if (pixmap_graphics_context_) {
    XFreeGC(display, pixmap_graphics_context_);
    pixmap_graphics_context_ = NULL;
  }
  if (pixmap_) {
    XFreePixmap(display, pixmap_);
    pixmap_ = 0;
  }

  // Recreate a pixmap to hold the frame.
  pixmap_ = XCreatePixmap(display,
                          window_,
                          window_size.width(),
                          window_size.height(),
                          attributes.depth);
  if (!pixmap_) {
    LOG(ERROR) << "XCreatePixmap failed.";
    return false;
  }

  // Recreate a graphics context for the pixmap.
  pixmap_graphics_context_ = XCreateGC(display, pixmap_, 0, NULL);
  if (!pixmap_graphics_context_) {
    LOG(ERROR) << "XCreateGC failed";
    return false;
  }

  return true;
}

GLContext* GLContext::CreateViewGLContext(gfx::PluginWindowHandle window,
                                          bool multisampled) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      scoped_ptr<ViewGLContext> context(new ViewGLContext(window));

      if (!context->Initialize(multisampled))
        return NULL;

      return context.release();
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<NativeViewEGLContext> context(
          new NativeViewEGLContext(reinterpret_cast<void *>(window)));
      if (!context->Initialize())
        return NULL;

      return context.release();
    }
    case kGLImplementationOSMesaGL: {
      scoped_ptr<OSMesaViewGLContext> context(new OSMesaViewGLContext(window));

      if (!context->Initialize())
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

bool PbufferGLContext::Initialize(GLContext* shared_context) {
  static const int config_attributes[] = {
    GLX_DRAWABLE_TYPE,
    GLX_PBUFFER_BIT,
    GLX_RENDER_TYPE,
    GLX_RGBA_BIT,
    GLX_DOUBLEBUFFER,
    0,
    0
  };

  Display* display = GetXDisplayHelper();

  int nelements = 0;
  // TODO(kbr): figure out whether hardcoding screen to 0 is sufficient.
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> config(
      glXChooseFBConfig(display, 0, config_attributes, &nelements));
  if (!config.get()) {
    LOG(ERROR) << "glXChooseFBConfig failed.";
    return false;
  }
  if (!nelements) {
    LOG(ERROR) << "glXChooseFBConfig returned 0 elements.";
    return false;
  }

  GLContextHandle shared_handle = NULL;
  if (shared_context)
    shared_handle = static_cast<GLContextHandle>(shared_context->GetHandle());

  context_ = glXCreateNewContext(display,
                                 config.get()[0],
                                 GLX_RGBA_TYPE,
                                 shared_handle,
                                 True);
  if (!context_) {
    LOG(ERROR) << "glXCreateNewContext failed.";
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
    LOG(ERROR) << "glXCreatePbuffer failed.";
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    LOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLContext::Destroy() {
  Display* display = GetXDisplayHelper();
  bool result = glXMakeCurrent(display, 0, 0);
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
    pbuffer_ = 0;
  }
}

bool PbufferGLContext::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }
  Display* display = GetXDisplayHelper();
  if (glXMakeCurrent(display, pbuffer_, context_) != True) {
    glXDestroyContext(display, context_);
    context_ = NULL;
    LOG(ERROR) << "Couldn't make context current.";
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

bool PbufferGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pbuffer.";
  return false;
}

gfx::Size PbufferGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of this pbuffer.";
  return gfx::Size(1, 1);
}

void* PbufferGLContext::GetHandle() {
  return context_;
}

void PbufferGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  NOTREACHED();
}

bool PixmapGLContext::Initialize(GLContext* shared_context) {
  VLOG(1) << "GL context: using pixmaps.";

  static int attributes[] = {
    GLX_RGBA,
    0
  };

  Display* display = GetXDisplayHelper();
  int screen = DefaultScreen(display);

  scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info(
      glXChooseVisual(display, screen, attributes));

  if (!visual_info.get()) {
    LOG(ERROR) << "glXChooseVisual failed.";
    return false;
  }

  GLContextHandle shared_handle = NULL;
  if (shared_context)
    shared_handle = static_cast<GLContextHandle>(shared_context->GetHandle());

  context_ = glXCreateContext(display, visual_info.get(), shared_handle, True);
  if (!context_) {
    LOG(ERROR) << "glXCreateContext failed.";
    return false;
  }

  pixmap_ = XCreatePixmap(display, RootWindow(display, screen), 1, 1,
                          visual_info->depth);
  if (!pixmap_) {
    LOG(ERROR) << "XCreatePixmap failed.";
    return false;
  }

  glx_pixmap_ = glXCreateGLXPixmap(display, visual_info.get(), pixmap_);
  if (!glx_pixmap_) {
    LOG(ERROR) << "XCreatePixmap failed.";
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    LOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void PixmapGLContext::Destroy() {
  Display* display = GetXDisplayHelper();
  bool result = glXMakeCurrent(display, 0, 0);
  // glXMakeCurrent isn't supposed to fail when unsetting the context, unless
  // we have pending draws on an invalid window - which shouldn't be the case
  // here.
  DCHECK(result);
  if (context_) {
    glXDestroyContext(display, context_);
    context_ = NULL;
  }

  if (glx_pixmap_) {
    glXDestroyGLXPixmap(display, glx_pixmap_);
    glx_pixmap_ = 0;
  }

  if (pixmap_) {
    XFreePixmap(display, pixmap_);
    pixmap_ = 0;
  }
}

bool PixmapGLContext::MakeCurrent() {
  if (IsCurrent()) {
    return true;
  }
  Display* display = GetXDisplayHelper();
  if (glXMakeCurrent(display, glx_pixmap_, context_) != True) {
    glXDestroyContext(display, context_);
    context_ = NULL;
    LOG(ERROR) << "Couldn't make context current.";
    return false;
  }

  return true;
}

bool PixmapGLContext::IsCurrent() {
  return glXGetCurrentDrawable() == glx_pixmap_ &&
      glXGetCurrentContext() == context_;
}

bool PixmapGLContext::IsOffscreen() {
  return true;
}

bool PixmapGLContext::SwapBuffers() {
  NOTREACHED() << "Attempted to call SwapBuffers on a pixmap.";
  return false;
}

gfx::Size PixmapGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of this pixmap.";
  return gfx::Size(1, 1);
}

void* PixmapGLContext::GetHandle() {
  return context_;
}

void PixmapGLContext::SetSwapInterval(int interval) {
  DCHECK(IsCurrent());
  NOTREACHED();
}

GLContext* GLContext::CreateOffscreenGLContext(GLContext* shared_context) {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL: {
      scoped_ptr<PbufferGLContext> context(new PbufferGLContext);
      if (context->Initialize(shared_context))
        return context.release();

      scoped_ptr<PixmapGLContext> context_pixmap(new PixmapGLContext);
      if (context_pixmap->Initialize(shared_context))
        return context_pixmap.release();

      return NULL;
    }
    case kGLImplementationEGLGLES2: {
      scoped_ptr<SecondaryEGLContext> context(
          new SecondaryEGLContext());
      if (!context->Initialize(shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationOSMesaGL: {
      scoped_ptr<OSMesaGLContext> context(new OSMesaGLContext);
      if (!context->Initialize(OSMESA_RGBA, shared_context))
        return NULL;

      return context.release();
    }
    case kGLImplementationMockGL:
      return new StubGLContext;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gfx
