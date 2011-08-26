// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/accelerated_surface_container_touch.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "base/memory/scoped_ptr.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace {

class AcceleratedSurfaceContainerTouchEGL
    : public AcceleratedSurfaceContainerTouch {
 public:
  AcceleratedSurfaceContainerTouchEGL(const gfx::Size& size,
                                      uint64 surface_handle);
  // TextureGL implementation
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

 private:
  ~AcceleratedSurfaceContainerTouchEGL();

  void* image_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerTouchEGL);
};

class AcceleratedSurfaceContainerTouchGLX
    : public AcceleratedSurfaceContainerTouch {
 public:
  AcceleratedSurfaceContainerTouchGLX(const gfx::Size& size,
                                      uint64 surface_handle);
  // TextureGL implementation
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

 private:
  ~AcceleratedSurfaceContainerTouchGLX();

  XID pixmap_;
  XID glx_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerTouchGLX);
};

class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

AcceleratedSurfaceContainerTouchEGL::AcceleratedSurfaceContainerTouchEGL(
    const gfx::Size& size,
    uint64 surface_handle)
    : AcceleratedSurfaceContainerTouch(size),
      image_(NULL) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  image_ = eglCreateImageKHR(
      gfx::GLSurfaceEGL::GetHardwareDisplay(), EGL_NO_CONTEXT,
      EGL_NATIVE_PIXMAP_KHR, reinterpret_cast<void*>(surface_handle), NULL);

  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
  glFlush();
}

AcceleratedSurfaceContainerTouchEGL::~AcceleratedSurfaceContainerTouchEGL() {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  eglDestroyImageKHR(gfx::GLSurfaceEGL::GetHardwareDisplay(), image_);
  glFlush();
}

void AcceleratedSurfaceContainerTouchEGL::Draw(
    const ui::TextureDrawParams& params,
    const gfx::Rect& clip_bounds_in_texture) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);

  ui::TextureDrawParams modified_params = params;

  // Texture from GPU is flipped vertically.
  ui::Transform flipped;
  flipped.SetScaleY(-1.0);
  flipped.SetTranslateY(size_.height());
  flipped.ConcatTransform(params.transform);

  modified_params.transform = flipped;

  DrawInternal(*instance->program_no_swizzle(),
               modified_params,
               clip_bounds_in_texture);
}

AcceleratedSurfaceContainerTouchGLX::AcceleratedSurfaceContainerTouchGLX(
    const gfx::Size& size,
    uint64 surface_handle)
    : AcceleratedSurfaceContainerTouch(size),
      pixmap_(0),
      glx_pixmap_(0) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  // Create pixmap from window.
  Display* dpy = gfx::GLSurfaceGLX::GetDisplay();
  int event_base, error_base;
  if (XCompositeQueryExtension(dpy, &event_base, &error_base)) {
    int major = 0, minor = 2;
    XCompositeQueryVersion(dpy, &major, &minor);
    if (major == 0 && minor < 2) {
      LOG(ERROR) << "Pixmap from window not supported.";
      return;
    }
  }
  pixmap_ = XCompositeNameWindowPixmap(dpy, surface_handle);

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
    return;
  }

  const int pixmapAttribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
    0
  };

  glx_pixmap_ = glXCreatePixmap(
      dpy, fbconfigs.get()[config], pixmap_, pixmapAttribs);

  // Create texture.
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

AcceleratedSurfaceContainerTouchGLX::~AcceleratedSurfaceContainerTouchGLX() {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  Display* dpy = gfx::GLSurfaceGLX::GetDisplay();
  if (glx_pixmap_)
    glXDestroyGLXPixmap(dpy, glx_pixmap_);
  if (pixmap_)
    XFreePixmap(dpy, pixmap_);
}

void AcceleratedSurfaceContainerTouchGLX::Draw(
    const ui::TextureDrawParams& params,
    const gfx::Rect& clip_bounds_in_texture) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);

  Display* dpy = gfx::GLSurfaceGLX::GetDisplay();

  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glXBindTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);
  DrawInternal(*instance->program_no_swizzle(),
               params,
               clip_bounds_in_texture);
  glXReleaseTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT);
}

}  // namespace

AcceleratedSurfaceContainerTouch::AcceleratedSurfaceContainerTouch(
    const gfx::Size& size) : TextureGL(size) {
}

// static
AcceleratedSurfaceContainerTouch*
AcceleratedSurfaceContainerTouch::CreateAcceleratedSurfaceContainer(
    const gfx::Size& size,
    uint64 surface_handle) {
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
      return new AcceleratedSurfaceContainerTouchGLX(size,
                                                     surface_handle);
    case gfx::kGLImplementationEGLGLES2:
      return new AcceleratedSurfaceContainerTouchEGL(size,
                                                     surface_handle);
    default:
      NOTREACHED();
      return NULL;
  }
}

void AcceleratedSurfaceContainerTouch::SetCanvas(
    const SkCanvas& canvas,
    const gfx::Point& origin,
    const gfx::Size& overall_size) {
  NOTREACHED();
}
