// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_client.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/renderer_host/image_transport_factory.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/scoped_make_current.h"

namespace {

class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

GLuint CreateTexture() {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return texture;
}

class ImageTransportClientEGL : public ImageTransportClient {
 public:
  ImageTransportClientEGL(ImageTransportFactory* factory, const gfx::Size& size)
      : ImageTransportClient(true, size),
        factory_(factory),
        image_(NULL) {
  }

  virtual ~ImageTransportClientEGL() {
    scoped_ptr<gfx::ScopedMakeCurrent> bind(factory_->GetScopedMakeCurrent());
    if (image_)
      eglDestroyImageKHR(gfx::GLSurfaceEGL::GetHardwareDisplay(), image_);
    unsigned int texture = texture_id();
    if (texture)
      glDeleteTextures(1, &texture);
    glFlush();
  }

  virtual bool Initialize(uint64* surface_handle) {
    scoped_ptr<gfx::ScopedMakeCurrent> bind(factory_->GetScopedMakeCurrent());
    image_ = eglCreateImageKHR(
        gfx::GLSurfaceEGL::GetHardwareDisplay(), EGL_NO_CONTEXT,
        EGL_NATIVE_PIXMAP_KHR, reinterpret_cast<void*>(*surface_handle), NULL);
    if (!image_)
      return false;
    set_texture_id(CreateTexture());
    glBindTexture(GL_TEXTURE_2D, texture_id());
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
    glFlush();
    return true;
  }

  virtual void Update() { }
  virtual TransportDIB::Handle Handle() const {
    return TransportDIB::DefaultHandleValue();
  }

 private:
  ImageTransportFactory* factory_;
  EGLImageKHR image_;
};

class ImageTransportClientGLX : public ImageTransportClient {
 public:
  ImageTransportClientGLX(ImageTransportFactory* factory, const gfx::Size& size)
      : ImageTransportClient(false, size),
        factory_(factory),
        pixmap_(0),
        glx_pixmap_(0),
        acquired_(false) {
  }

  virtual ~ImageTransportClientGLX() {
    scoped_ptr<gfx::ScopedMakeCurrent> bind(factory_->GetScopedMakeCurrent());
    Display* dpy = base::MessagePumpForUI::GetDefaultXDisplay();
    if (glx_pixmap_) {
      if (acquired_)
        glXReleaseTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT);
      glXDestroyGLXPixmap(dpy, glx_pixmap_);
    }
    if (pixmap_)
      XFreePixmap(dpy, pixmap_);
    unsigned int texture = texture_id();
    if (texture)
      glDeleteTextures(1, &texture);
    glFlush();
  }

  virtual bool Initialize(uint64* surface_handle) {
    TRACE_EVENT0("renderer_host", "ImageTransportClientGLX::Initialize");
    Display* dpy = base::MessagePumpForUI::GetDefaultXDisplay();

    scoped_ptr<gfx::ScopedMakeCurrent> bind(factory_->GetScopedMakeCurrent());
    if (!InitializeOneOff(dpy))
      return false;

    // Create pixmap from window.
    // We receive a window here rather than a pixmap directly because drivers
    // require (or required) that the pixmap used to create the GL texture be
    // created in the same process as the texture.
    pixmap_ = XCompositeNameWindowPixmap(dpy, *surface_handle);

    const int pixmapAttribs[] = {
      GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
      GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
      0
    };

    glx_pixmap_ = glXCreatePixmap(dpy, fbconfig_.Get(), pixmap_, pixmapAttribs);

    set_texture_id(CreateTexture());
    glFlush();
    return true;
  }

  virtual void Update() {
    TRACE_EVENT0("renderer_host", "ImageTransportClientGLX::Update");
    Display* dpy = base::MessagePumpForUI::GetDefaultXDisplay();
    scoped_ptr<gfx::ScopedMakeCurrent> bind(factory_->GetScopedMakeCurrent());
    glBindTexture(GL_TEXTURE_2D, texture_id());
    if (acquired_)
      glXReleaseTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT);
    glXBindTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);
    acquired_ = true;
    glFlush();
  }

  virtual TransportDIB::Handle Handle() const {
    return TransportDIB::DefaultHandleValue();
  }

 private:
  static bool InitializeOneOff(Display* dpy) {
    static bool initialized = false;
    if (initialized)
      return true;

    int event_base, error_base;
    if (XCompositeQueryExtension(dpy, &event_base, &error_base)) {
      int major = 0, minor = 2;
      XCompositeQueryVersion(dpy, &major, &minor);
      if (major == 0 && minor < 2) {
        LOG(ERROR) << "Pixmap from window not supported.";
        return false;
      }
    }
    // Wrap the pixmap in a GLXPixmap
    int screen = DefaultScreen(dpy);
    XWindowAttributes gwa;
    XGetWindowAttributes(dpy, RootWindow(dpy, screen), &gwa);
    unsigned int visualid = XVisualIDFromVisual(gwa.visual);

    int nfbconfigs, config;
    scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> fbconfigs(
        glXGetFBConfigs(dpy, screen, &nfbconfigs));

    for (config = 0; config < nfbconfigs; config++) {
      XVisualInfo* visinfo = glXGetVisualFromFBConfig(
          dpy, fbconfigs.get()[config]);
      if (!visinfo || visinfo->visualid != visualid)
        continue;

      int value;
      glXGetFBConfigAttrib(dpy,
          fbconfigs.get()[config],
          GLX_DRAWABLE_TYPE,
          &value);
      if (!(value & GLX_PIXMAP_BIT))
        continue;

      glXGetFBConfigAttrib(dpy,
          fbconfigs.get()[config],
          GLX_BIND_TO_TEXTURE_TARGETS_EXT,
          &value);
      if (!(value & GLX_TEXTURE_2D_BIT_EXT))
        continue;

      glXGetFBConfigAttrib(dpy,
          fbconfigs.get()[config],
          GLX_BIND_TO_TEXTURE_RGB_EXT,
          &value);
      if (value == GL_FALSE)
        continue;

      break;
    }

    if (config == nfbconfigs) {
      LOG(ERROR)
        << "Could not find configuration suitable for binding a pixmap "
        << "as a texture.";
      return false;
    }
    fbconfig_.Get() = fbconfigs.get()[config];
    initialized = true;
    return initialized;
  }

  ImageTransportFactory* factory_;
  XID pixmap_;
  XID glx_pixmap_;
  bool acquired_;
  static base::LazyInstance<GLXFBConfig> fbconfig_;
};

base::LazyInstance<GLXFBConfig> ImageTransportClientGLX::fbconfig_ =
    LAZY_INSTANCE_INITIALIZER;

class ImageTransportClientOSMesa : public ImageTransportClient {
 public:
  ImageTransportClientOSMesa(ImageTransportFactory* factory,
                             const gfx::Size& size)
      : ImageTransportClient(false, size),
        factory_(factory) {
  }

  virtual ~ImageTransportClientOSMesa() {
    unsigned int texture = texture_id();
    if (texture) {
      scoped_ptr<gfx::ScopedMakeCurrent> bind(
          factory_->GetScopedMakeCurrent());
      glDeleteTextures(1, &texture);
      glFlush();
    }
  }

  virtual bool Initialize(uint64* surface_handle) {
    // We expect to make the handle here, so don't want the other end giving us
    // one.
    DCHECK_EQ(*surface_handle, static_cast<uint64>(0));

    // It's possible that this ID gneration could clash with IDs from other
    // AcceleratedSurfaceContainerTouch* objects, however we should never have
    // ids active from more than one type at the same time, so we have free
    // reign of the id namespace.
    *surface_handle = next_handle_++;

    shared_mem_.reset(
        TransportDIB::Create(size().GetArea() * 4,  // GL_RGBA=4 B/px
        *surface_handle));
    if (!shared_mem_.get())
      return false;

    scoped_ptr<gfx::ScopedMakeCurrent> bind(factory_->GetScopedMakeCurrent());
    set_texture_id(CreateTexture());
    glFlush();
    return true;
  }

  virtual void Update() {
    glBindTexture(GL_TEXTURE_2D, texture_id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 size().width(), size().height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, shared_mem_->memory());
    glFlush();
  }

  virtual TransportDIB::Handle Handle() const { return shared_mem_->handle(); }

 private:
  ImageTransportFactory* factory_;
  scoped_ptr<TransportDIB> shared_mem_;
  static uint32 next_handle_;
};
uint32 ImageTransportClientOSMesa::next_handle_ = 0;

}  // anonymous namespace

ImageTransportClient* ImageTransportClient::Create(
    ImageTransportFactory* factory,
    const gfx::Size& size) {
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationOSMesaGL:
      return new ImageTransportClientOSMesa(factory, size);
    case gfx::kGLImplementationDesktopGL:
      return new ImageTransportClientGLX(factory, size);
    case gfx::kGLImplementationEGLGLES2:
      return new ImageTransportClientEGL(factory, size);
    default:
      NOTREACHED();
      return NULL;
  }
}
