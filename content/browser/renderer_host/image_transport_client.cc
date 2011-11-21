// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_client.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/gl/scoped_make_current.h"
#include "ui/gfx/size.h"

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
  explicit ImageTransportClientEGL(ui::SharedResources* resources)
      : resources_(resources),
        image_(NULL) {
  }

  virtual ~ImageTransportClientEGL() {
    if (image_) {
      scoped_ptr<gfx::ScopedMakeCurrent> bind(
          resources_->GetScopedMakeCurrent());
      eglDestroyImageKHR(gfx::GLSurfaceEGL::GetHardwareDisplay(), image_);
      glFlush();
    }
  }

  virtual unsigned int Initialize(uint64* surface_id) {
    scoped_ptr<gfx::ScopedMakeCurrent> bind(resources_->GetScopedMakeCurrent());
    image_ = eglCreateImageKHR(
        gfx::GLSurfaceEGL::GetHardwareDisplay(), EGL_NO_CONTEXT,
        EGL_NATIVE_PIXMAP_KHR, reinterpret_cast<void*>(*surface_id), NULL);
    if (!image_)
      return 0;
    GLuint texture = CreateTexture();
    glBindTexture(GL_TEXTURE_2D, texture);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
    glFlush();
    return texture;
  }

  virtual void Acquire() { }
  virtual void Release() { }
  virtual bool Flipped() { return true; }
  virtual TransportDIB::Handle Handle() const {
    return TransportDIB::DefaultHandleValue();
  }

 private:
  ui::SharedResources* resources_;
  EGLImageKHR image_;
};

#if !defined(USE_WAYLAND)

class ImageTransportClientGLX : public ImageTransportClient {
 public:
  explicit ImageTransportClientGLX(ui::SharedResources* resources)
      : resources_(resources),
        pixmap_(0),
        glx_pixmap_(0),
        texture_(0) {
  }

  virtual ~ImageTransportClientGLX() {
    Display* dpy = static_cast<Display*>(resources_->GetDisplay());
    if (glx_pixmap_)
      glXDestroyGLXPixmap(dpy, glx_pixmap_);
    if (pixmap_)
      XFreePixmap(dpy, pixmap_);
  }

  virtual unsigned int Initialize(uint64* surface_id) {
    TRACE_EVENT0("renderer_host", "ImageTransportClientGLX::Initialize");
    Display* dpy = static_cast<Display*>(resources_->GetDisplay());

    scoped_ptr<gfx::ScopedMakeCurrent> bind(resources_->GetScopedMakeCurrent());
    if (!InitializeOneOff(dpy))
        return 0;

    // Create pixmap from window.
    // We receive a window here rather than a pixmap directly because drivers
    // require (or required) that the pixmap used to create the GL texture be
    // created in the same process as the texture.
    pixmap_ = XCompositeNameWindowPixmap(dpy, *surface_id);

    const int pixmapAttribs[] = {
      GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
      GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
      0
    };

    glx_pixmap_ = glXCreatePixmap(dpy, fbconfig_.Get(), pixmap_, pixmapAttribs);

    texture_ = CreateTexture();
    return texture_;
  }

  virtual void Acquire() {
    TRACE_EVENT0("renderer_host", "ImageTransportClientGLX::Acquire");
    Display* dpy = static_cast<Display*>(resources_->GetDisplay());
    glBindTexture(GL_TEXTURE_2D, texture_);
    glXBindTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);
  }

  virtual void Release() {
    TRACE_EVENT0("renderer_host", "ImageTransportClientGLX::Release");
    Display* dpy = static_cast<Display*>(resources_->GetDisplay());
    glXReleaseTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT);
  }

  virtual bool Flipped() { return false; }
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

  ui::SharedResources* resources_;
  XID pixmap_;
  XID glx_pixmap_;
  GLuint texture_;
  static base::LazyInstance<GLXFBConfig> fbconfig_;
};

base::LazyInstance<GLXFBConfig> ImageTransportClientGLX::fbconfig_ =
    LAZY_INSTANCE_INITIALIZER;

class ImageTransportClientOSMesa : public ImageTransportClient {
 public:
  ImageTransportClientOSMesa(ui::SharedResources* resources,
                             const gfx::Size& size)
      : resources_(resources),
        size_(size),
        texture_(0) {
  }

  virtual ~ImageTransportClientOSMesa() {
  }

  virtual unsigned int Initialize(uint64* surface_id) {
    // We expect to make the id here, so don't want the other end giving us one
    DCHECK_EQ(*surface_id, static_cast<uint64>(0));

    // It's possible that this ID gneration could clash with IDs from other
    // AcceleratedSurfaceContainerTouch* objects, however we should never have
    // ids active from more than one type at the same time, so we have free
    // reign of the id namespace.
    *surface_id = next_id_++;

    shared_mem_.reset(
        TransportDIB::Create(size_.GetArea() * 4,  // GL_RGBA=4 B/px
        *surface_id));
    if (!shared_mem_.get())
      return 0;

    scoped_ptr<gfx::ScopedMakeCurrent> bind(resources_->GetScopedMakeCurrent());
    texture_ = CreateTexture();
    return texture_;
  }

  virtual void Acquire() {
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 size_.width(), size_.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, shared_mem_->memory());
  }

  virtual void Release() { }
  virtual bool Flipped() { return false; }
  virtual TransportDIB::Handle Handle() const { return shared_mem_->handle(); }

 private:
  ui::SharedResources* resources_;
  gfx::Size size_;
  scoped_ptr<TransportDIB> shared_mem_;
  GLuint texture_;
  static uint32 next_id_;
};
uint32 ImageTransportClientOSMesa::next_id_ = 0;

#endif //  !USE_WAYLAND

}  // anonymous namespace

ImageTransportClient* ImageTransportClient::Create(
    ui::SharedResources* resources,
    const gfx::Size& size) {
  switch (gfx::GetGLImplementation()) {
#if !defined(USE_WAYLAND)
    case gfx::kGLImplementationOSMesaGL:
      return new ImageTransportClientOSMesa(resources, size);
    case gfx::kGLImplementationDesktopGL:
      return new ImageTransportClientGLX(resources);
#endif
    case gfx::kGLImplementationEGLGLES2:
      return new ImageTransportClientEGL(resources);
    default:
      NOTREACHED();
      return NULL;
  }
}
