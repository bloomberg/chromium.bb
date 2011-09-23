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
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_surface_glx.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"
#include "ui/gfx/surface/accelerated_surface_linux.h"

namespace {

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
  virtual void OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnNewSurfaceACK(
      uint64 surface_id, TransportDIB::Handle surface_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~EGLImageTransportSurface() OVERRIDE;
  void ReleaseSurface(scoped_refptr<AcceleratedSurface>* surface);

  uint32 fbo_id_;

  scoped_refptr<AcceleratedSurface> back_surface_;
  scoped_refptr<AcceleratedSurface> front_surface_;

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
  virtual void OnMakeCurrent(gfx::GLContext* context) OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  virtual void OnNewSurfaceACK(
      uint64 surface_id, TransportDIB::Handle surface_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~GLXImageTransportSurface();

  // Tell the browser to release the surface.
  void ReleaseSurface();

  XID dummy_parent_;
  gfx::Size size_;

  // Whether or not the image has been bound on the browser side.
  bool bound_;

  // Whether or not we've set the swap interval on the associated context.
  bool swap_interval_set_;

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
  virtual void* GetHandle() OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  virtual void OnNewSurfaceACK(
      uint64 surface_id, TransportDIB::Handle surface_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
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

EGLImageTransportSurface::EGLImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id)
      : gfx::PbufferGLSurfaceEGL(false, gfx::Size(1, 1)),
        fbo_id_(0) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id));
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

void EGLImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (fbo_id_)
    return;

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
  OnResize(gfx::Size(1, 1));

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Framebuffer incomplete.";
  }
}

unsigned int EGLImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

void EGLImageTransportSurface::ReleaseSurface(
    scoped_refptr<AcceleratedSurface>* surface) {
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

  back_surface_ = new AcceleratedSurface(size);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            back_surface_->texture(),
                            0);
  glFlush();

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size.width();
  params.height = size.height();
  params.identifier = back_surface_->pixmap();
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

bool EGLImageTransportSurface::SwapBuffers() {
  front_surface_.swap(back_surface_);
  DCHECK_NE(front_surface_.get(), static_cast<AcceleratedSurface*>(NULL));
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

GLXImageTransportSurface::GLXImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id)
      : gfx::NativeViewGLSurfaceGLX(),
        dummy_parent_(0),
        size_(1, 1),
        bound_(false),
        swap_interval_set_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id));
}

GLXImageTransportSurface::~GLXImageTransportSurface() {
  Destroy();
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
  OnResize(size_);

  if (!helper_->Initialize())
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

  helper_->Destroy();
  gfx::NativeViewGLSurfaceGLX::Destroy();
}

void GLXImageTransportSurface::ReleaseSurface() {
  DCHECK(bound_);
  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  params.identifier = window_;
  helper_->SendAcceleratedSurfaceRelease(params);
}

void GLXImageTransportSurface::OnResize(gfx::Size size) {
  size_ = size;
  if (bound_) {
    ReleaseSurface();
    bound_ = false;
  }

  Display* dpy = gfx::GLSurfaceGLX::GetDisplay();
  XResizeWindow(dpy, window_, size_.width(), size_.height());
  XFlush(dpy);

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size_.width();
  params.height = size_.height();
  params.identifier = window_;
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

bool GLXImageTransportSurface::SwapBuffers() {
  gfx::NativeViewGLSurfaceGLX::SwapBuffers();
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = window_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

gfx::Size GLXImageTransportSurface::GetSize() {
  return size_;
}

void GLXImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (!swap_interval_set_) {
    context->SetSwapInterval(0);
    swap_interval_set_ = true;
  }
}

void GLXImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle /*surface_handle*/) {
  DCHECK(!bound_);
  bound_ = true;
  helper_->SetScheduled(true);
}

void GLXImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
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
                                         command_buffer_id));
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

  // Now that the shared memory buffer is gone, we need to change OSMesa to
  // point at something valid, so we'll let it point to the buffer in the super
  // class.  This would be really bad since that buffer has a different size,
  // but we don't allow any drawing to take place until we reset the surface
  // back to a new buffer of shared memory.
  helper_->MakeCurrent();

  size_ = size;

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size_.width();
  params.height = size_.height();
  params.identifier = 0; // id comes from the browser with the shared mem
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

void OSMesaImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle surface_handle) {
  shared_id_ = surface_id;
  shared_mem_.reset(TransportDIB::Map(surface_handle));
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));

  // When we get the shared memory buffer back we can use that for OSMesa to
  // write in, so we give it to OSMesa.
  helper_->MakeCurrent();

  helper_->SetScheduled(true);
}

bool OSMesaImageTransportSurface::SwapBuffers() {
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));

  // Copy the OSMesa buffer to the shared memory
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

void* OSMesaImageTransportSurface::GetHandle() {
  if (shared_mem_.get())
    return shared_mem_->memory();
  else
    return GLSurfaceOSMesa::GetHandle();
}

}  // namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id) {
  scoped_refptr<gfx::GLSurface> surface;
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
  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

ImageTransportHelper::ImageTransportHelper(ImageTransportSurface* surface,
                                           GpuChannelManager* manager,
                                           int32 render_view_id,
                                           int32 renderer_id,
                                           int32 command_buffer_id)
    : surface_(surface),
      manager_(manager),
      render_view_id_(render_view_id),
      renderer_id_(renderer_id),
      command_buffer_id_(command_buffer_id) {
  route_id_ = manager_->GenerateRouteID();
  manager_->AddRoute(route_id_, this);
}

ImageTransportHelper::~ImageTransportHelper() {
  manager_->RemoveRoute(route_id_);
}

bool ImageTransportHelper::Initialize() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();
  if (!decoder)
    return false;

  decoder->SetResizeCallback(
       NewCallback(this, &ImageTransportHelper::Resize));

  return true;
}

void ImageTransportHelper::Destroy() {
}

bool ImageTransportHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageTransportHelper, message)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_NewACK,
                        OnNewSurfaceACK)
    IPC_MESSAGE_HANDLER(AcceleratedSurfaceMsg_BuffersSwappedACK,
                        OnBuffersSwappedACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageTransportHelper::SendAcceleratedSurfaceRelease(
    GpuHostMsg_AcceleratedSurfaceRelease_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceRelease(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceNew(
    GpuHostMsg_AcceleratedSurfaceNew_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceNew(params));
}

void ImageTransportHelper::SendAcceleratedSurfaceBuffersSwapped(
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params) {
  params.renderer_id = renderer_id_;
  params.render_view_id = render_view_id_;
  params.route_id = route_id_;
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceBuffersSwapped(params));
}

void ImageTransportHelper::SetScheduled(bool is_scheduled) {
  gpu::GpuScheduler* scheduler = Scheduler();
  if (!scheduler)
    return;

  scheduler->SetScheduled(is_scheduled);
}

void ImageTransportHelper::OnNewSurfaceACK(
    uint64 surface_id, TransportDIB::Handle surface_handle) {
  surface_->OnNewSurfaceACK(surface_id, surface_handle);
}

void ImageTransportHelper::OnBuffersSwappedACK() {
  surface_->OnBuffersSwappedACK();
}

void ImageTransportHelper::Resize(gfx::Size size) {
  surface_->OnResize(size);
}

bool ImageTransportHelper::MakeCurrent() {
  gpu::gles2::GLES2Decoder* decoder = Decoder();
  if (!decoder)
    return false;
  return decoder->MakeCurrent();
}

gpu::GpuScheduler* ImageTransportHelper::Scheduler() {
  GpuChannel* channel = manager_->LookupChannel(renderer_id_);
  if (!channel)
    return NULL;

  GpuCommandBufferStub* stub =
      channel->LookupCommandBuffer(command_buffer_id_);
  if (!stub)
    return NULL;

  return stub->scheduler();
}

gpu::gles2::GLES2Decoder* ImageTransportHelper::Decoder() {
  GpuChannel* channel = manager_->LookupChannel(renderer_id_);
  if (!channel)
    return NULL;

  GpuCommandBufferStub* stub =
      channel->LookupCommandBuffer(command_buffer_id_);
  if (!stub)
    return NULL;

  return stub->decoder();
}

#endif  // defined(USE_GPU)
