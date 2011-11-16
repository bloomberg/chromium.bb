// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

// This conflicts with the defines in Xlib.h and must come first.
#include "content/common/gpu/gpu_messages.h"

#include <map>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"

namespace {

// The GL context associated with the surface must be current when
// an instance is created or destroyed.
class EGLAcceleratedSurface : public base::RefCounted<EGLAcceleratedSurface> {
 public:
  EGLAcceleratedSurface(const gfx::Size& size);
  const gfx::Size& size() const { return size_; }
  uint32 pixmap() const { return pixmap_; }
  uint32 texture() const { return texture_; }

 private:
  ~EGLAcceleratedSurface();

  gfx::Size size_;
  void* image_;
  uint32 pixmap_;
  uint32 texture_;

  friend class base::RefCounted<EGLAcceleratedSurface>;
  DISALLOW_COPY_AND_ASSIGN(EGLAcceleratedSurface);
};

// We are backed by an Pbuffer offscreen surface for the purposes of creating a
// context, but use FBOs to render to X Pixmap backed EGLImages.
class EGLImageTransportSurface : public ImageTransportSurface,
                                 public gfx::PbufferGLSurfaceEGL {
 public:
  EGLImageTransportSurface(GpuChannelManager* manager,
                           int32 render_view_id,
                           int32 renderer_id,
                           int32 command_buffer_id);

  // GLSurface implementation
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnNewSurfaceACK(
      uint64 surface_id, TransportDIB::Handle surface_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~EGLImageTransportSurface() OVERRIDE;
  void ReleaseSurface(scoped_refptr<EGLAcceleratedSurface>* surface);

  uint32 fbo_id_;

  scoped_refptr<EGLAcceleratedSurface> back_surface_;
  scoped_refptr<EGLAcceleratedSurface> front_surface_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(EGLImageTransportSurface);
};

// We render to an off-screen (but mapped) window that the browser process will
// read from via XComposite
class GLXImageTransportSurface : public ImageTransportSurface,
                                 public gfx::NativeViewGLSurfaceGLX {
 public:
  GLXImageTransportSurface(GpuChannelManager* manager,
                           int32 render_view_id,
                           int32 renderer_id,
                           int32 command_buffer_id);

  // gfx::GLSurface implementation:
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  virtual void OnNewSurfaceACK(
      uint64 surface_id, TransportDIB::Handle surface_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~GLXImageTransportSurface();

  // Tell the browser to release the surface.
  void ReleaseSurface();

  XID dummy_parent_;
  gfx::Size size_;

  // Whether or not the image has been bound on the browser side.
  bool bound_;

  // Whether or not we need to send a resize on the next swap.
  bool needs_resize_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(GLXImageTransportSurface);
};

// We render to a hunk of shared memory that we get from the browser.
// Swapping buffers simply means telling the browser to read the contents
// of the memory.
class OSMesaImageTransportSurface : public ImageTransportSurface,
                                    public gfx::GLSurfaceOSMesa {
 public:
  OSMesaImageTransportSurface(GpuChannelManager* manager,
                              int32 render_view_id,
                              int32 renderer_id,
                              int32 command_buffer_id);

  // gfx::GLSurface implementation:
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  virtual void OnNewSurfaceACK(
      uint64 surface_id, TransportDIB::Handle surface_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~OSMesaImageTransportSurface();

  // Tell the browser to release the surface.
  void ReleaseSurface();

  scoped_ptr<TransportDIB> shared_mem_;
  uint32 shared_id_;
  gfx::Size size_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(OSMesaImageTransportSurface);
};

EGLAcceleratedSurface::EGLAcceleratedSurface(const gfx::Size& size)
    : size_(size), texture_(0) {
  Display* dpy = gfx::GLSurfaceEGL::GetNativeDisplay();
  EGLDisplay edpy = gfx::GLSurfaceEGL::GetHardwareDisplay();

  XID window = XDefaultRootWindow(dpy);
  XWindowAttributes gwa;
  XGetWindowAttributes(dpy, window, &gwa);
  pixmap_ = XCreatePixmap(
      dpy, window, size_.width(), size_.height(), gwa.depth);

  image_ = eglCreateImageKHR(
      edpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
      reinterpret_cast<void*>(pixmap_), NULL);

  glGenTextures(1, &texture_);

  GLint current_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);

  glBindTexture(GL_TEXTURE_2D, current_texture);
}

EGLAcceleratedSurface::~EGLAcceleratedSurface() {
  glDeleteTextures(1, &texture_);
  eglDestroyImageKHR(gfx::GLSurfaceEGL::GetHardwareDisplay(), image_);
  XFreePixmap(gfx::GLSurfaceEGL::GetNativeDisplay(), pixmap_);
}

EGLImageTransportSurface::EGLImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id)
      : gfx::PbufferGLSurfaceEGL(false, gfx::Size(1, 1)),
        fbo_id_(0),
        made_current_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
                                         gfx::kNullPluginWindow));
}

EGLImageTransportSurface::~EGLImageTransportSurface() {
  Destroy();
}

bool EGLImageTransportSurface::Initialize() {
  if (!helper_->Initialize())
    return false;
  return PbufferGLSurfaceEGL::Initialize();
}

void EGLImageTransportSurface::Destroy() {
  if (back_surface_.get())
    ReleaseSurface(&back_surface_);
  if (front_surface_.get())
    ReleaseSurface(&front_surface_);

  helper_->Destroy();
  PbufferGLSurfaceEGL::Destroy();
}

// Make sure that buffer swaps occur for the surface, so we can send the data
// to the actual onscreen surface in the browser
bool EGLImageTransportSurface::IsOffscreen() {
  return false;
}

bool EGLImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (made_current_)
    return true;

  if (!context->HasExtension("EGL_KHR_image") &&
      !context->HasExtension("EGL_KHR_image_pixmap")) {
    DLOG(ERROR) << "EGLImage from X11 pixmap not supported";
    return false;
  }

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
  OnResize(gfx::Size(1, 1));

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "Framebuffer incomplete.";
    return false;
  }

  made_current_ = true;
  return true;
}

unsigned int EGLImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

void EGLImageTransportSurface::SetVisible(bool visible) {
  if (!visible && back_surface_.get() && front_surface_.get()) {
    ReleaseSurface(&back_surface_);
  } else if (visible && !back_surface_.get() && front_surface_.get()) {
    // Leverage the OnResize hook because it does exactly what we want
    OnResize(front_surface_->size());
  }
}

void EGLImageTransportSurface::ReleaseSurface(
    scoped_refptr<EGLAcceleratedSurface>* surface) {
  if (surface->get()) {
    GpuHostMsg_AcceleratedSurfaceRelease_Params params;
    params.identifier = (*surface)->pixmap();
    helper_->SendAcceleratedSurfaceRelease(params);
    *surface = NULL;
  }
}

void EGLImageTransportSurface::OnResize(gfx::Size size) {
  if (back_surface_.get())
    ReleaseSurface(&back_surface_);

  back_surface_ = new EGLAcceleratedSurface(size);

  GLint previous_fbo_id = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo_id);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            back_surface_->texture(),
                            0);
  glFlush();

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previous_fbo_id);

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size.width();
  params.height = size.height();
  params.surface_id = back_surface_->pixmap();
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

bool EGLImageTransportSurface::SwapBuffers() {
  front_surface_.swap(back_surface_);
  DCHECK_NE(front_surface_.get(), static_cast<EGLAcceleratedSurface*>(NULL));
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = front_surface_->pixmap();
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  gfx::Size expected_size = front_surface_->size();
  if (!back_surface_.get() || back_surface_->size() != expected_size) {
    OnResize(expected_size);
  } else {
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              back_surface_->texture(),
                              0);
  }
  helper_->SetScheduled(false);
  return true;
}

gfx::Size EGLImageTransportSurface::GetSize() {
  return back_surface_->size();
}

void EGLImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle /*surface_handle*/) {
  DCHECK_EQ(back_surface_->pixmap(), surface_id);
  helper_->SetScheduled(true);
}

void EGLImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}


void EGLImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

GLXImageTransportSurface::GLXImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id)
      : gfx::NativeViewGLSurfaceGLX(),
        dummy_parent_(0),
        size_(1, 1),
        bound_(false),
        needs_resize_(false),
        made_current_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
                                         gfx::kNullPluginWindow));
}

GLXImageTransportSurface::~GLXImageTransportSurface() {
  Destroy();
}

bool GLXImageTransportSurface::Initialize() {
  // Create a dummy window to host the real window.
  Display* dpy = static_cast<Display*>(GetDisplay());
  XSetWindowAttributes swa;
  swa.event_mask = StructureNotifyMask;
  swa.override_redirect = True;
  dummy_parent_ = XCreateWindow(
      dpy,
      RootWindow(dpy, DefaultScreen(dpy)),  // parent
      -100, -100, 1, 1,
      0,  // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWEventMask | CWOverrideRedirect, &swa);
  XMapWindow(dpy, dummy_parent_);

  swa.event_mask = StructureNotifyMask;
  swa.override_redirect = false;
  window_ = XCreateWindow(dpy,
                          dummy_parent_,
                          0, 0, size_.width(), size_.height(),
                          0,  // border width
                          CopyFromParent,  // depth
                          InputOutput,
                          CopyFromParent,  // visual
                          CWEventMask, &swa);
  XMapWindow(dpy, window_);
  while (1) {
    XEvent event;
    XNextEvent(dpy, &event);
    if (event.type == MapNotify && event.xmap.window == window_)
      break;
  }
  // Manual setting must be used to avoid unnecessary rendering by server.
  XCompositeRedirectWindow(dpy, window_, CompositeRedirectManual);
  OnResize(size_);

  if (!helper_->Initialize())
    return false;
  return gfx::NativeViewGLSurfaceGLX::Initialize();
}

void GLXImageTransportSurface::Destroy() {
  if (bound_)
    ReleaseSurface();

  if (window_) {
    Display* dpy = static_cast<Display*>(GetDisplay());
    XDestroyWindow(dpy, window_);
    XDestroyWindow(dpy, dummy_parent_);
  }

  helper_->Destroy();
  gfx::NativeViewGLSurfaceGLX::Destroy();
}

void GLXImageTransportSurface::ReleaseSurface() {
  DCHECK(bound_);
  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  params.identifier = window_;
  helper_->SendAcceleratedSurfaceRelease(params);
  bound_ = false;
}

void GLXImageTransportSurface::OnResize(gfx::Size size) {
  TRACE_EVENT0("gpu", "GLXImageTransportSurface::OnResize");
  size_ = size;

  Display* dpy = static_cast<Display*>(GetDisplay());
  XResizeWindow(dpy, window_, size_.width(), size_.height());
  glXWaitX();
  needs_resize_ = true;
}

bool GLXImageTransportSurface::SwapBuffers() {
  gfx::NativeViewGLSurfaceGLX::SwapBuffers();
  glFlush();

  if (needs_resize_) {
    GpuHostMsg_AcceleratedSurfaceNew_Params params;
    params.width = size_.width();
    params.height = size_.height();
    params.surface_id = window_;
    helper_->SendAcceleratedSurfaceNew(params);
    bound_ = true;
    needs_resize_ = false;
  }

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = window_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

gfx::Size GLXImageTransportSurface::GetSize() {
  return size_;
}

bool GLXImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (made_current_)
    return true;

  // Check for driver support.
  Display* dpy = static_cast<Display*>(GetDisplay());
  int event_base, error_base;
  if (XCompositeQueryExtension(dpy, &event_base, &error_base)) {
    int major = 0, minor = 2;
    XCompositeQueryVersion(dpy, &major, &minor);
    if (major == 0 && minor < 2) {
      DLOG(ERROR) << "Pixmap from window not supported.";
      return false;
    }
  }

  context->SetSwapInterval(0);

  made_current_ = true;
  return true;
}

void GLXImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle /*surface_handle*/) {
}

void GLXImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void GLXImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

OSMesaImageTransportSurface::OSMesaImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id)
  : gfx::GLSurfaceOSMesa(OSMESA_RGBA, gfx::Size(1, 1)),
    size_(gfx::Size(1, 1)) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
                                         gfx::kNullPluginWindow));
}

OSMesaImageTransportSurface::~OSMesaImageTransportSurface() {
  Destroy();
}

bool OSMesaImageTransportSurface::Initialize() {
  if (!helper_->Initialize())
    return false;
  return gfx::GLSurfaceOSMesa::Initialize();
}

void OSMesaImageTransportSurface::Destroy() {
  if (shared_mem_.get())
    ReleaseSurface();

  helper_->Destroy();
  gfx::GLSurfaceOSMesa::Destroy();
}

// Make sure that buffer swaps occur for the surface, so we can send the data
// to the actual onscreen surface in the browser
bool OSMesaImageTransportSurface::IsOffscreen() {
  return false;
}

void OSMesaImageTransportSurface::ReleaseSurface() {
  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  params.identifier = shared_id_;
  helper_->SendAcceleratedSurfaceRelease(params);

  shared_mem_.reset();
  shared_id_ = 0;
}

void OSMesaImageTransportSurface::OnResize(gfx::Size size) {
  if (shared_mem_.get())
    ReleaseSurface();

  GLSurfaceOSMesa::Resize(size);

  // Now that we resized/reallocated the memory buffer, we need to change
  // what OSMesa is pointing at to the new buffer.
  helper_->MakeCurrent();

  size_ = size;

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size_.width();
  params.height = size_.height();
  params.surface_id = 0;  // id comes from the browser with the shared mem
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

void OSMesaImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle surface_handle) {
  shared_id_ = surface_id;
  shared_mem_.reset(TransportDIB::Map(surface_handle));
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));

  helper_->SetScheduled(true);
}

void OSMesaImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

bool OSMesaImageTransportSurface::SwapBuffers() {
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));

  // Copy the OSMesa buffer to the shared memory
  glFinish();
  memcpy(shared_mem_->memory(), GetHandle(), size_.GetArea() * 4);

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = shared_id_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

void OSMesaImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

gfx::Size OSMesaImageTransportSurface::GetSize() {
  return size_;
}

}  // namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id,
    gfx::PluginWindowHandle handle) {
  scoped_refptr<gfx::GLSurface> surface;
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
      surface = new GLXImageTransportSurface(manager,
                                             render_view_id,
                                             renderer_id,
                                             command_buffer_id);
      break;
    case gfx::kGLImplementationEGLGLES2:
      surface = new EGLImageTransportSurface(manager,
                                             render_view_id,
                                             renderer_id,
                                             command_buffer_id);
      break;
    case gfx::kGLImplementationOSMesaGL:
      surface = new OSMesaImageTransportSurface(manager,
                                                render_view_id,
                                                renderer_id,
                                                command_buffer_id);
      break;
    default:
      NOTREACHED();
      return NULL;
  }
#else
  surface = gfx::GLSurface::CreateViewGLSurface(false, handle);
  if (!surface.get())
    return NULL;

  surface = new PassThroughImageTransportSurface(manager,
                                                 render_view_id,
                                                 renderer_id,
                                                 command_buffer_id,
                                                 surface.get());
#endif
  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

#endif  // defined(USE_GPU)
