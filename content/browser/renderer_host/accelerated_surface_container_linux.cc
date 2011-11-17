// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/accelerated_surface_container_linux.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "base/lazy_instance.h"
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

class AcceleratedSurfaceContainerLinuxEGL
    : public AcceleratedSurfaceContainerLinux {
 public:
  explicit AcceleratedSurfaceContainerLinuxEGL(const gfx::Size& size);

  virtual bool Initialize(uint64* surface_id) OVERRIDE;

  // TextureGL implementation
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

 private:
  virtual ~AcceleratedSurfaceContainerLinuxEGL();

  void* image_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerLinuxEGL);
};

class AcceleratedSurfaceContainerLinuxGLX
    : public AcceleratedSurfaceContainerLinux {
 public:
  explicit AcceleratedSurfaceContainerLinuxGLX(const gfx::Size& size);

  virtual bool Initialize(uint64* surface_id) OVERRIDE;

  // TextureGL implementation
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

 protected:
  static bool InitializeOneOff();

  static base::LazyInstance<GLXFBConfig> fbconfig_;

 private:
  virtual ~AcceleratedSurfaceContainerLinuxGLX();

  XID pixmap_;
  XID glx_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerLinuxGLX);
};

class AcceleratedSurfaceContainerLinuxOSMesa
    : public AcceleratedSurfaceContainerLinux {
 public:
  explicit AcceleratedSurfaceContainerLinuxOSMesa(const gfx::Size& size);

  virtual bool Initialize(uint64* surface_id) OVERRIDE;

  // TextureGL implementation
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

  // Some implementations of this class use shared memory, this gives the handle
  // to the shared buffer, which is part of the surface container.
  // When shared memory is not used, this will return
  // TransportDIB::DefaultHandleValue().
  virtual TransportDIB::Handle Handle() const OVERRIDE;

 private:
  virtual ~AcceleratedSurfaceContainerLinuxOSMesa();

  scoped_ptr<TransportDIB> shared_mem_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurfaceContainerLinuxOSMesa);
};

class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

// static
base::LazyInstance<GLXFBConfig> AcceleratedSurfaceContainerLinuxGLX::fbconfig_(
    base::LINKER_INITIALIZED);

AcceleratedSurfaceContainerLinuxEGL::AcceleratedSurfaceContainerLinuxEGL(
    const gfx::Size& size)
    : AcceleratedSurfaceContainerLinux(size),
      image_(NULL) {
}

bool AcceleratedSurfaceContainerLinuxEGL::Initialize(uint64* surface_id) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  image_ = eglCreateImageKHR(
      gfx::GLSurfaceEGL::GetHardwareDisplay(), EGL_NO_CONTEXT,
      EGL_NATIVE_PIXMAP_KHR, reinterpret_cast<void*>(*surface_id), NULL);

  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
  glFlush();

  return true;
}

AcceleratedSurfaceContainerLinuxEGL::~AcceleratedSurfaceContainerLinuxEGL() {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  eglDestroyImageKHR(gfx::GLSurfaceEGL::GetHardwareDisplay(), image_);
  glFlush();
}

void AcceleratedSurfaceContainerLinuxEGL::Draw(
    const ui::TextureDrawParams& params,
    const gfx::Rect& clip_bounds_in_texture) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);

  ui::TextureDrawParams modified_params = params;
  modified_params.vertically_flipped = true;

  DrawInternal(*instance->program_no_swizzle(),
               modified_params,
               clip_bounds_in_texture);
}

AcceleratedSurfaceContainerLinuxGLX::AcceleratedSurfaceContainerLinuxGLX(
    const gfx::Size& size)
    : AcceleratedSurfaceContainerLinux(size),
      pixmap_(0),
      glx_pixmap_(0) {
}

bool AcceleratedSurfaceContainerLinuxGLX::Initialize(uint64* surface_id) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  if (!AcceleratedSurfaceContainerLinuxGLX::InitializeOneOff())
    return false;

  // Create pixmap from window.
  // We receive a window here rather than a pixmap directly because drivers
  // require (or required) that the pixmap used to create the GL texture be
  // created in the same process as the texture.
  Display* dpy = static_cast<Display*>(instance->GetDisplay());
  pixmap_ = XCompositeNameWindowPixmap(dpy, *surface_id);

  // Wrap the pixmap in a GLXPixmap
  const int pixmapAttribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
    0
  };

  glx_pixmap_ = glXCreatePixmap(
      dpy, fbconfig_.Get(), pixmap_, pixmapAttribs);

  // Create texture.
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  return true;
}

AcceleratedSurfaceContainerLinuxGLX::~AcceleratedSurfaceContainerLinuxGLX() {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  Display* dpy = static_cast<Display*>(instance->GetDisplay());
  if (glx_pixmap_)
    glXDestroyGLXPixmap(dpy, glx_pixmap_);
  if (pixmap_)
    XFreePixmap(dpy, pixmap_);
}

void AcceleratedSurfaceContainerLinuxGLX::Draw(
    const ui::TextureDrawParams& params,
    const gfx::Rect& clip_bounds_in_texture) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);

  Display* dpy = static_cast<Display*>(instance->GetDisplay());

  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glXBindTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);
  DrawInternal(*instance->program_no_swizzle(),
               params,
               clip_bounds_in_texture);
  glXReleaseTexImageEXT(dpy, glx_pixmap_, GLX_FRONT_LEFT_EXT);
}

// static
bool AcceleratedSurfaceContainerLinuxGLX::InitializeOneOff()
{
  static bool initialized = false;
  if (initialized)
    return true;

  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);

  Display* dpy = static_cast<Display*>(instance->GetDisplay());
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

AcceleratedSurfaceContainerLinuxOSMesa::AcceleratedSurfaceContainerLinuxOSMesa(
    const gfx::Size& size)
      : AcceleratedSurfaceContainerLinux(size) {
}

bool AcceleratedSurfaceContainerLinuxOSMesa::Initialize(uint64* surface_id) {
  static uint32 next_id = 1;

  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);
  instance->MakeSharedContextCurrent();

  // We expect to make the id here, so don't want the other end giving us one
  DCHECK_EQ(*surface_id, static_cast<uint64>(0));

  // It's possible that this ID gneration could clash with IDs from other
  // AcceleratedSurfaceContainerLinux* objects, however we should never have
  // ids active from more than one type at the same time, so we have free
  // reign of the id namespace.
  *surface_id = next_id++;

  shared_mem_.reset(
      TransportDIB::Create(size_.GetArea() * 4,  // GL_RGBA=4 B/px
                           *surface_id));
  // Create texture.
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // we generate the ID to be used here.
  return true;
}

void AcceleratedSurfaceContainerLinuxOSMesa::Draw(
    const ui::TextureDrawParams& params,
    const gfx::Rect& clip_bounds_in_texture) {
  ui::SharedResources* instance = ui::SharedResources::GetInstance();
  DCHECK(instance);

  if (shared_mem_.get()) {
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 size_.width(), size_.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, shared_mem_->memory());

    DrawInternal(*instance->program_no_swizzle(),
                 params,
                 clip_bounds_in_texture);
  }
}

TransportDIB::Handle AcceleratedSurfaceContainerLinuxOSMesa::Handle() const {
  if (shared_mem_.get())
    return shared_mem_->handle();
  else
    return TransportDIB::DefaultHandleValue();
}

AcceleratedSurfaceContainerLinuxOSMesa::
    ~AcceleratedSurfaceContainerLinuxOSMesa() {
}

}  // namespace

AcceleratedSurfaceContainerLinux::AcceleratedSurfaceContainerLinux(
    const gfx::Size& size) : TextureGL(size) {
}

TransportDIB::Handle AcceleratedSurfaceContainerLinux::Handle() const {
  return TransportDIB::DefaultHandleValue();
}

// static
AcceleratedSurfaceContainerLinux*
AcceleratedSurfaceContainerLinux::CreateAcceleratedSurfaceContainer(
    const gfx::Size& size) {
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
      return new AcceleratedSurfaceContainerLinuxGLX(size);
    case gfx::kGLImplementationEGLGLES2:
      return new AcceleratedSurfaceContainerLinuxEGL(size);
    case gfx::kGLImplementationOSMesaGL:
      return new AcceleratedSurfaceContainerLinuxOSMesa(size);
    default:
      NOTREACHED();
      return NULL;
  }
}

void AcceleratedSurfaceContainerLinux::SetCanvas(
    const SkCanvas& canvas,
    const gfx::Point& origin,
    const gfx::Size& overall_size) {
  NOTREACHED();
}

