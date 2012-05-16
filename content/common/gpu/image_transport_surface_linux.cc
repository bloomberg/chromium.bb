// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

// This conflicts with the defines in Xlib.h and must come first.
#include "content/common/gpu/gpu_messages.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <map>
#include <vector>

// Note: these must be included before anything that includes gl_bindings.h
// They're effectively standard library headers.
#include "third_party/khronos/EGL/egl.h"
#include "third_party/khronos/EGL/eglext.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/texture_image_transport_surface.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/rect.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_surface_osmesa.h"

namespace {

class ScopedDisplayLock {
 public:
  ScopedDisplayLock(Display* display): display_(display) {
    XLockDisplay(display_);
  }

  ~ScopedDisplayLock() {
    XUnlockDisplay(display_);
  }

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisplayLock);
};

// The GL context associated with the surface must be current when
// an instance is created or destroyed.
class EGLAcceleratedSurface : public base::RefCounted<EGLAcceleratedSurface> {
 public:
  explicit EGLAcceleratedSurface(const gfx::Size& size);
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
class EGLImageTransportSurface
    : public ImageTransportSurface,
      public gfx::PbufferGLSurfaceEGL,
      public base::SupportsWeakPtr<EGLImageTransportSurface> {
 public:
  EGLImageTransportSurface(GpuChannelManager* manager,
                           GpuCommandBufferStub* stub);

  // gfx::GLSurface implementation
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual void SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnNewSurfaceACK(
      uint64 surface_handle, TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~EGLImageTransportSurface() OVERRIDE;
  void ReleaseSurface(scoped_refptr<EGLAcceleratedSurface>* surface);
  void SendBuffersSwapped();
  void SendPostSubBuffer(int x, int y, int width, int height);

  // Tracks the current buffer allocation state.
  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  // The expected size when visible.
  gfx::Size visible_size_;

  uint32 fbo_id_;

  scoped_refptr<EGLAcceleratedSurface> back_surface_;
  scoped_refptr<EGLAcceleratedSurface> front_surface_;
  gfx::Rect previous_damage_rect_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(EGLImageTransportSurface);
};

// We render to an off-screen (but mapped) window that the browser process will
// read from via XComposite
class GLXImageTransportSurface
    : public ImageTransportSurface,
      public gfx::NativeViewGLSurfaceGLX,
      public base::SupportsWeakPtr<GLXImageTransportSurface> {
 public:
  GLXImageTransportSurface(GpuChannelManager* manager,
                           GpuCommandBufferStub* stub);

  // gfx::GLSurface implementation:
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual std::string GetExtensions();
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual void SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  virtual void OnNewSurfaceACK(
      uint64 surface_handle, TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~GLXImageTransportSurface();

  // Tell the browser to release the surface.
  void ReleaseSurface();

  void SendBuffersSwapped();
  void SendPostSubBuffer(int x, int y, int width, int height);

  void ResizeSurface(gfx::Size size);

  // Tracks the current buffer allocation state.
  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

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
                              GpuCommandBufferStub* stub);

  // gfx::GLSurface implementation:
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;

 protected:
  // ImageTransportSurface implementation:
  virtual void OnNewSurfaceACK(
      uint64 surface_handle, TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
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
  bool success = XGetWindowAttributes(dpy, window, &gwa);
  DCHECK(success);
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
    GpuCommandBufferStub* stub)
    : gfx::PbufferGLSurfaceEGL(false, gfx::Size(16, 16)),
      backbuffer_suggested_allocation_(true),
      frontbuffer_suggested_allocation_(true),
      fbo_id_(0),
      made_current_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

EGLImageTransportSurface::~EGLImageTransportSurface() {
  Destroy();
}

bool EGLImageTransportSurface::Initialize() {
  if (!helper_->Initialize())
    return false;
  return gfx::PbufferGLSurfaceEGL::Initialize();
}

void EGLImageTransportSurface::Destroy() {
  if (back_surface_.get())
    ReleaseSurface(&back_surface_);
  if (front_surface_.get())
    ReleaseSurface(&front_surface_);

  helper_->Destroy();
  gfx::PbufferGLSurfaceEGL::Destroy();
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

  // Creating 16x16 (instead of 1x1) to work around ARM Mali driver issue
  // (see https://code.google.com/p/chrome-os-partner/issues/detail?id=9445)
  OnResize(gfx::Size(16, 16));

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

void EGLImageTransportSurface::SetBackbufferAllocation(bool allocated) {
  if (backbuffer_suggested_allocation_ == allocated)
    return;
  backbuffer_suggested_allocation_ = allocated;

  if (backbuffer_suggested_allocation_)
    OnResize(visible_size_);
  else
    ReleaseSurface(&back_surface_);
}

void EGLImageTransportSurface::SetFrontbufferAllocation(bool allocated) {
  if (frontbuffer_suggested_allocation_ == allocated)
    return;
  frontbuffer_suggested_allocation_ = allocated;
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
  visible_size_ = size;
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
  params.surface_handle = back_surface_->pixmap();
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

bool EGLImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_) {
    previous_damage_rect_ = gfx::Rect(visible_size_);
    return true;
  }
  front_surface_.swap(back_surface_);
  DCHECK_NE(front_surface_.get(), static_cast<EGLAcceleratedSurface*>(NULL));
  helper_->DeferToFence(base::Bind(
      &EGLImageTransportSurface::SendBuffersSwapped,
      AsWeakPtr()));

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
  previous_damage_rect_ = gfx::Rect(front_surface_->size());
  return true;
}

void EGLImageTransportSurface::SendBuffersSwapped() {
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = front_surface_->pixmap();
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);
  helper_->SetScheduled(false);
}

bool EGLImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_) {
    previous_damage_rect_ = gfx::Rect(visible_size_);
    return true;
  }
  DCHECK_NE(back_surface_.get(), static_cast<EGLAcceleratedSurface*>(NULL));
  gfx::Size expected_size = back_surface_->size();
  bool surfaces_same_size = front_surface_.get() &&
      front_surface_->size() == expected_size;

  const gfx::Rect new_damage_rect(x, y, width, height);

  // An empty damage rect is a successful no-op.
  if (new_damage_rect.IsEmpty())
    return true;

  if (surfaces_same_size) {
    std::vector<gfx::Rect> regions_to_copy;
    GetRegionsToCopy(previous_damage_rect_, new_damage_rect, &regions_to_copy);

    GLint previous_texture_id = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_texture_id);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              front_surface_->texture(),
                              0);
    glBindTexture(GL_TEXTURE_2D, back_surface_->texture());

    for (size_t i = 0; i < regions_to_copy.size(); ++i) {
      const gfx::Rect& region_to_copy = regions_to_copy[i];
      if (!region_to_copy.IsEmpty()) {
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, region_to_copy.x(),
            region_to_copy.y(), region_to_copy.x(), region_to_copy.y(),
            region_to_copy.width(), region_to_copy.height());
      }
    }
    glBindTexture(GL_TEXTURE_2D, previous_texture_id);
  }

  front_surface_.swap(back_surface_);

  if (!surfaces_same_size) {
    DCHECK(new_damage_rect == gfx::Rect(expected_size));
    OnResize(expected_size);
  }

  helper_->DeferToFence(base::Bind(
      &EGLImageTransportSurface::SendPostSubBuffer,
      AsWeakPtr(), x, y, width, height));

  previous_damage_rect_ = new_damage_rect;

  return true;
}

void EGLImageTransportSurface::SendPostSubBuffer(
    int x, int y, int width, int height) {
  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = front_surface_->pixmap();
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;

  helper_->SendAcceleratedSurfacePostSubBuffer(params);
  helper_->SetScheduled(false);
}

std::string EGLImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached ";
  extensions += "GL_CHROMIUM_post_sub_buffer";
  return extensions;
}

gfx::Size EGLImageTransportSurface::GetSize() {
  return back_surface_->size();
}

void EGLImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_handle, TransportDIB::Handle /*shm_handle*/) {
  DCHECK_EQ(back_surface_->pixmap(), surface_handle);
  helper_->SetScheduled(true);
}

void EGLImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void EGLImageTransportSurface::OnPostSubBufferACK() {
  helper_->SetScheduled(true);
}

void EGLImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

GLXImageTransportSurface::GLXImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub)
    : gfx::NativeViewGLSurfaceGLX(),
      backbuffer_suggested_allocation_(true),
      frontbuffer_suggested_allocation_(true),
      dummy_parent_(0),
      size_(1, 1),
      bound_(false),
      needs_resize_(false),
      made_current_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

GLXImageTransportSurface::~GLXImageTransportSurface() {
  Destroy();
}

bool GLXImageTransportSurface::Initialize() {
  // Create a dummy window to host the real window.
  Display* dpy = static_cast<Display*>(GetDisplay());
  ScopedDisplayLock lock(dpy);

  XSetWindowAttributes swa;
  swa.override_redirect = True;
  dummy_parent_ = XCreateWindow(
      dpy,
      RootWindow(dpy, DefaultScreen(dpy)),  // parent
      -100, -100, 1, 1,
      0,  // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWOverrideRedirect, &swa);
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
    XWindowEvent(dpy, window_, StructureNotifyMask, &event);
    if (event.type == MapNotify && event.xmap.window == window_)
      break;
  }
  XSelectInput(dpy, window_, NoEventMask);

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

void GLXImageTransportSurface::SetBackbufferAllocation(bool allocated) {
  if (backbuffer_suggested_allocation_ == allocated)
    return;
  backbuffer_suggested_allocation_ = allocated;

  if (backbuffer_suggested_allocation_)
    ResizeSurface(size_);
  else
    ResizeSurface(gfx::Size(1,1));
}

void GLXImageTransportSurface::SetFrontbufferAllocation(bool allocated) {
  if (frontbuffer_suggested_allocation_ == allocated)
    return;
  frontbuffer_suggested_allocation_ = allocated;

  // We recreate frontbuffer by recreating backbuffer and swapping.
  // But we release frontbuffer by telling UI to release its handle on it.
  if (!frontbuffer_suggested_allocation_ && bound_)
    ReleaseSurface();
}

void GLXImageTransportSurface::ResizeSurface(gfx::Size size) {
  Display* dpy = static_cast<Display*>(GetDisplay());
  XResizeWindow(dpy, window_, size.width(), size.height());
  glXWaitX();
  // Seems necessary to perform a swap after a resize
  // in order to resize the front and back buffers (Intel driver bug).
  // This doesn't always happen with scissoring enabled, so do it now.
  if (gfx::g_GLX_MESA_copy_sub_buffer && gfx::GLSurface::GetCurrent() == this)
    gfx::NativeViewGLSurfaceGLX::SwapBuffers();
  needs_resize_ = true;
}

void GLXImageTransportSurface::OnResize(gfx::Size size) {
  TRACE_EVENT0("gpu", "GLXImageTransportSurface::OnResize");
  size_ = size;
  ResizeSurface(size_);
}

bool GLXImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  gfx::NativeViewGLSurfaceGLX::SwapBuffers();
  helper_->DeferToFence(base::Bind(
      &GLXImageTransportSurface::SendBuffersSwapped,
      AsWeakPtr()));

  if (needs_resize_) {
    GpuHostMsg_AcceleratedSurfaceNew_Params params;
    params.width = size_.width();
    params.height = size_.height();
    params.surface_handle = window_;
    helper_->SendAcceleratedSurfaceNew(params);
    bound_ = true;
    needs_resize_ = false;
  }
  return true;
}

void GLXImageTransportSurface::SendBuffersSwapped() {
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = window_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);
  helper_->SetScheduled(false);
}

bool GLXImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  gfx::NativeViewGLSurfaceGLX::PostSubBuffer(x, y, width, height);
  helper_->DeferToFence(base::Bind(
      &GLXImageTransportSurface::SendPostSubBuffer,
      AsWeakPtr(), x, y, width, height));

  if (needs_resize_) {
    GpuHostMsg_AcceleratedSurfaceNew_Params params;
    params.width = size_.width();
    params.height = size_.height();
    params.surface_handle = window_;
    helper_->SendAcceleratedSurfaceNew(params);
    bound_ = true;
    needs_resize_ = false;
  }
  return true;
}

void GLXImageTransportSurface::SendPostSubBuffer(
    int x, int y, int width, int height) {
  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = window_;
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;

  helper_->SendAcceleratedSurfacePostSubBuffer(params);
  helper_->SetScheduled(false);
}

std::string GLXImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::NativeViewGLSurfaceGLX::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached";
  return extensions;
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
    uint64 surface_handle, TransportDIB::Handle /*shm_handle*/) {
}

void GLXImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void GLXImageTransportSurface::OnPostSubBufferACK() {
  helper_->SetScheduled(true);
}

void GLXImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

OSMesaImageTransportSurface::OSMesaImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub)
    : gfx::GLSurfaceOSMesa(OSMESA_RGBA, gfx::Size(1, 1)),
      size_(gfx::Size(1, 1)) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
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
  shared_mem_.reset();
  shared_id_ = 0;

  GLSurfaceOSMesa::Resize(size);

  // Now that we resized/reallocated the memory buffer, we need to change
  // what OSMesa is pointing at to the new buffer.
  helper_->MakeCurrent();

  size_ = size;

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size_.width();
  params.height = size_.height();
  params.surface_handle = 0;  // id comes from the browser with the shared mem
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

void OSMesaImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_handle, TransportDIB::Handle shm_handle) {
  shared_id_ = surface_handle;
  shared_mem_.reset(TransportDIB::Map(shm_handle));
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
  params.surface_handle = shared_id_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

bool OSMesaImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));

  // Copy the OSMesa buffer to the shared memory
  glFinish();

  int flipped_y = GetSize().height() - y - height;

  for (int row = 0; row < height; ++row) {
    int mem_offset = ((flipped_y + row) * size_.width() + x);
    int32* dest_address = static_cast<int32*>(shared_mem_->memory()) +
        mem_offset;
    int32* src_address = static_cast<int32*>(GetHandle()) + mem_offset;
    memcpy(dest_address, src_address, width * 4);
  }

  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = shared_id_;
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  helper_->SetScheduled(false);
  return true;
}

std::string OSMesaImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached ";
  extensions += "GL_CHROMIUM_post_sub_buffer";
  return extensions;
}

void OSMesaImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void OSMesaImageTransportSurface::OnPostSubBufferACK() {
  helper_->SetScheduled(true);
}

gfx::Size OSMesaImageTransportSurface::GetSize() {
  return size_;
}

}  // namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  scoped_refptr<gfx::GLSurface> surface;
  if (!handle.handle) {
    DCHECK(handle.transport);
    if (!handle.parent_client_id) {
      switch (gfx::GetGLImplementation()) {
        case gfx::kGLImplementationDesktopGL:
          surface = new GLXImageTransportSurface(manager, stub);
          break;
        case gfx::kGLImplementationEGLGLES2:
          surface = new EGLImageTransportSurface(manager, stub);
          break;
        case gfx::kGLImplementationOSMesaGL:
          surface = new OSMesaImageTransportSurface(manager, stub);
          break;
        default:
          NOTREACHED();
          return NULL;
      }
    } else {
      surface = new TextureImageTransportSurface(manager, stub, handle);
    }
  } else {
    surface = gfx::GLSurface::CreateViewGLSurface(false, handle.handle);
    if (!surface.get())
      return NULL;
    surface = new PassThroughImageTransportSurface(manager,
                                                   stub,
                                                   surface.get(),
                                                   handle.transport);
  }

  if (surface->Initialize())
    return surface;
  else
    return NULL;
}
