// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface_linux.h"

// This conflicts with the defines in Xlib.h and must come first.
#include "content/common/gpu/gpu_messages.h"

#include <map>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "base/callback.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/surface/accelerated_surface_linux.h"

namespace {

// We are backed by an Pbuffer offscreen surface for the purposes of creating a
// context, but use FBOs to render to X Pixmap backed EGLImages.
class EGLImageTransportSurface : public ImageTransportSurface,
                                 public gfx::PbufferGLSurfaceEGL {
 public:
  explicit EGLImageTransportSurface(GpuCommandBufferStub* stub);

  // GLSurface implementation
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnSetSurfaceACK(uint64 surface_id) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void Resize(gfx::Size size) OVERRIDE;

 private:
  virtual ~EGLImageTransportSurface() OVERRIDE;
  void ReleaseSurface(scoped_refptr<AcceleratedSurface>& surface);

  uint32 fbo_id_;

  scoped_refptr<AcceleratedSurface> back_surface_;
  scoped_refptr<AcceleratedSurface> front_surface_;

  DISALLOW_COPY_AND_ASSIGN(EGLImageTransportSurface);
};

// We are backed by an Pbuffer offscreen surface for the purposes of creating a
// context, but use FBOs to render to X Pixmap backed EGLImages.
class GLXImageTransportSurface : public ImageTransportSurface,
                                 public gfx::NativeViewGLSurfaceGLX {
 public:
  explicit GLXImageTransportSurface(GpuCommandBufferStub* stub);

  // gfx::GLSurface implementation:
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  void OnSetSurfaceACK(uint64 surface_id) OVERRIDE;
  void OnBuffersSwappedACK() OVERRIDE;
  void Resize(gfx::Size size) OVERRIDE;

 private:
  virtual ~GLXImageTransportSurface();

  // Tell the browser to release the surface.
  void ReleaseSurface();

  XID dummy_parent_;
  gfx::Size size_;

  // Whether or not the image has been bound on the browser side.
  bool bound_;

  DISALLOW_COPY_AND_ASSIGN(GLXImageTransportSurface);
};

EGLImageTransportSurface::EGLImageTransportSurface(GpuCommandBufferStub* stub)
    : ImageTransportSurface(stub),
      gfx::PbufferGLSurfaceEGL(false, gfx::Size(1, 1)),
      fbo_id_(0) {
}

EGLImageTransportSurface::~EGLImageTransportSurface() {
}

bool EGLImageTransportSurface::Initialize() {
  if (!ImageTransportSurface::Initialize())
    return false;
  return PbufferGLSurfaceEGL::Initialize();
}

void EGLImageTransportSurface::Destroy() {
  if (back_surface_.get())
    ReleaseSurface(back_surface_);
  if (front_surface_.get())
    ReleaseSurface(front_surface_);

  ImageTransportSurface::Destroy();
  PbufferGLSurfaceEGL::Destroy();
}

bool EGLImageTransportSurface::IsOffscreen() {
  return false;
}

void EGLImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (fbo_id_)
    return;

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
  Resize(gfx::Size(1, 1));

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Framebuffer incomplete.";
  }
}

unsigned int EGLImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

void EGLImageTransportSurface::ReleaseSurface(
    scoped_refptr<AcceleratedSurface>& surface) {
  if (surface.get()) {
    GpuHostMsg_AcceleratedSurfaceRelease_Params params;
    params.renderer_id = stub()->renderer_id();
    params.render_view_id = stub()->render_view_id();
    params.identifier = back_surface_->pixmap();
    params.route_id = route_id();
    Send(new GpuHostMsg_AcceleratedSurfaceRelease(params));
    surface = NULL;
  }
}

void EGLImageTransportSurface::Resize(gfx::Size size) {
  if (back_surface_.get())
    ReleaseSurface(back_surface_);

  back_surface_ = new AcceleratedSurface(size);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            back_surface_->texture(),
                            0);
  glFlush();

  GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params;
  params.renderer_id = stub()->renderer_id();
  params.render_view_id = stub()->render_view_id();
  params.width = size.width();
  params.height = size.height();
  params.identifier = back_surface_->pixmap();
  params.route_id = route_id();
  Send(new GpuHostMsg_AcceleratedSurfaceSetIOSurface(params));

  scheduler()->SetScheduled(false);
}

bool EGLImageTransportSurface::SwapBuffers() {
  front_surface_.swap(back_surface_);
  DCHECK_NE(front_surface_.get(), static_cast<AcceleratedSurface*>(NULL));
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.renderer_id = stub()->renderer_id();
  params.render_view_id = stub()->render_view_id();
  params.surface_id = front_surface_->pixmap();
  params.route_id = route_id();
  Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));

  gfx::Size expected_size = front_surface_->size();
  if (!back_surface_.get() || back_surface_->size() != expected_size) {
    Resize(expected_size);
  } else {
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              back_surface_->texture(),
                              0);
  }
  scheduler()->SetScheduled(false);
  return true;
}

gfx::Size EGLImageTransportSurface::GetSize() {
  return back_surface_->size();
}

void EGLImageTransportSurface::OnSetSurfaceACK(
    uint64 surface_id) {
  DCHECK_EQ(back_surface_->pixmap(), surface_id);
  scheduler()->SetScheduled(true);
}

void EGLImageTransportSurface::OnBuffersSwappedACK() {
  scheduler()->SetScheduled(true);
}

GLXImageTransportSurface::GLXImageTransportSurface(GpuCommandBufferStub* stub)
     : ImageTransportSurface(stub),
       gfx::NativeViewGLSurfaceGLX(),
       dummy_parent_(0),
       size_(1, 1),
       bound_(false) {
}

GLXImageTransportSurface::~GLXImageTransportSurface() {
}

bool GLXImageTransportSurface::Initialize() {
  // Create a dummy window to host the real window.
  Display* dpy = gfx::GLSurfaceGLX::GetDisplay();
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
  Resize(size_);

  if (!ImageTransportSurface::Initialize())
    return false;
  return gfx::NativeViewGLSurfaceGLX::Initialize();
}

void GLXImageTransportSurface::Destroy() {
  if (bound_)
    ReleaseSurface();

  if (window_) {
    Display* dpy = gfx::GLSurfaceGLX::GetDisplay();
    XDestroyWindow(dpy, window_);
    XDestroyWindow(dpy, dummy_parent_);
  }

  ImageTransportSurface::Destroy();
  gfx::NativeViewGLSurfaceGLX::Destroy();
}

void GLXImageTransportSurface::ReleaseSurface() {
  DCHECK(bound_);
  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  params.renderer_id = stub()->renderer_id();
  params.render_view_id = stub()->render_view_id();
  params.identifier = window_;
  params.route_id = route_id();
  Send(new GpuHostMsg_AcceleratedSurfaceRelease(params));
}

void GLXImageTransportSurface::Resize(gfx::Size size) {
  size_ = size;
  if (bound_) {
    ReleaseSurface();
    bound_ = false;
  }

  Display* dpy = gfx::GLSurfaceGLX::GetDisplay();
  XResizeWindow(dpy, window_, size_.width(), size_.height());
  XFlush(dpy);

  GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params;
  params.renderer_id = stub()->renderer_id();
  params.render_view_id = stub()->render_view_id();
  params.width = size_.width();
  params.height = size_.height();
  params.identifier = window_;
  params.route_id = route_id();
  Send(new GpuHostMsg_AcceleratedSurfaceSetIOSurface(params));

  scheduler()->SetScheduled(false);
}

bool GLXImageTransportSurface::SwapBuffers() {
  gfx::NativeViewGLSurfaceGLX::SwapBuffers();
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.renderer_id = stub()->renderer_id();
  params.render_view_id = stub()->render_view_id();
  params.surface_id = window_;
  params.route_id = route_id();
  Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));

  return true;
}

gfx::Size GLXImageTransportSurface::GetSize() {
  return size_;
}

void GLXImageTransportSurface::OnSetSurfaceACK(
    uint64 surface_id) {
  DCHECK(!bound_);
  bound_ = true;
  scheduler()->SetScheduled(true);
}

void GLXImageTransportSurface::OnBuffersSwappedACK() {
}

}  // namespace

ImageTransportSurface::ImageTransportSurface(GpuCommandBufferStub* stub)
    : stub_(stub) {
  GpuChannelManager* gpu_channel_manager
      = stub_->channel()->gpu_channel_manager();
  route_id_ = gpu_channel_manager->GenerateRouteID();
  gpu_channel_manager->AddRoute(route_id_, this);
}

ImageTransportSurface::~ImageTransportSurface() {
  GpuChannelManager* gpu_channel_manager
      = stub_->channel()->gpu_channel_manager();
  gpu_channel_manager->RemoveRoute(route_id_);
}

bool ImageTransportSurface::Initialize() {
  scheduler()->SetResizeCallback(
     NewCallback(this, &ImageTransportSurface::Resize));
  return true;
}

void ImageTransportSurface::Destroy() {
  scheduler()->SetResizeCallback(NULL);
}

bool ImageTransportSurface::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageTransportSurface, message)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_SetSurfaceACK,
                        OnSetSurfaceACK)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_BuffersSwappedACK,
                        OnBuffersSwappedACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool ImageTransportSurface::Send(IPC::Message* message) {
  GpuChannelManager* gpu_channel_manager =
      stub_->channel()->gpu_channel_manager();
  return gpu_channel_manager->Send(message);
}

gpu::GpuScheduler* ImageTransportSurface::scheduler() {
  return stub_->scheduler();
}

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuCommandBufferStub* stub) {
  scoped_refptr<gfx::GLSurface> surface;
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
      surface = new GLXImageTransportSurface(stub);
      break;
    case gfx::kGLImplementationEGLGLES2:
      surface = new EGLImageTransportSurface(stub);
      break;
    default:
      NOTREACHED();
      return NULL;
  }
  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

#endif  // defined(USE_GPU)
