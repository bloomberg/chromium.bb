// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_cgl.h"
#include "ui/surface/io_surface_support_mac.h"

namespace {

// We are backed by an offscreen surface for the purposes of creating
// a context, but use FBOs to render to texture backed IOSurface
class IOSurfaceImageTransportSurface : public gfx::NoOpGLSurfaceCGL,
                                       public ImageTransportSurface {
 public:
  IOSurfaceImageTransportSurface(GpuChannelManager* manager,
                                 GpuCommandBufferStub* stub,
                                 gfx::PluginWindowHandle handle);

  // GLSurface implementation
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
  virtual void OnNewSurfaceACK(uint64 surface_handle,
                               TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~IOSurfaceImageTransportSurface() OVERRIDE;

  void UnrefIOSurface();
  void CreateIOSurface();

  // Tracks the current buffer allocation state.
  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  uint32 fbo_id_;
  GLuint texture_id_;

  base::mac::ScopedCFTypeRef<CFTypeRef> io_surface_;

  // The id of |io_surface_| or 0 if that's NULL.
  uint64 io_surface_handle_;

  // Weak pointer to the context that this was last made current to.
  gfx::GLContext* context_;

  gfx::Size size_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceImageTransportSurface);
};

void AddBooleanValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     bool value) {
  CFDictionaryAddValue(dictionary, key,
                       (value ? kCFBooleanTrue : kCFBooleanFalse));
}

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32 value) {
  base::mac::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

IOSurfaceImageTransportSurface::IOSurfaceImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle)
    : gfx::NoOpGLSurfaceCGL(gfx::Size(1, 1)),
      backbuffer_suggested_allocation_(true),
      frontbuffer_suggested_allocation_(true),
      fbo_id_(0),
      texture_id_(0),
      io_surface_handle_(0),
      context_(NULL),
      made_current_(false) {
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
}

IOSurfaceImageTransportSurface::~IOSurfaceImageTransportSurface() {
  Destroy();
}

bool IOSurfaceImageTransportSurface::Initialize() {
  // Only support IOSurfaces if the GL implementation is the native desktop GL.
  // IO surfaces will not work with, for example, OSMesa software renderer
  // GL contexts.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL &&
      gfx::GetGLImplementation() != gfx::kGLImplementationAppleGL)
    return false;

  if (!helper_->Initialize())
    return false;
  return NoOpGLSurfaceCGL::Initialize();
}

void IOSurfaceImageTransportSurface::Destroy() {
  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    fbo_id_ = 0;
  }

  if (texture_id_) {
    glDeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }

  helper_->Destroy();
  NoOpGLSurfaceCGL::Destroy();
}

bool IOSurfaceImageTransportSurface::IsOffscreen() {
  return false;
}

bool IOSurfaceImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  context_ = context;

  if (made_current_)
    return true;

  OnResize(gfx::Size(1, 1));

  made_current_ = true;
  return true;
}

unsigned int IOSurfaceImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

void IOSurfaceImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  if (backbuffer_suggested_allocation_ == allocation)
    return;
  backbuffer_suggested_allocation_ = allocation;

  if (backbuffer_suggested_allocation_)
    CreateIOSurface();
  else
    UnrefIOSurface();
}

void IOSurfaceImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;

  // We recreate frontbuffer by recreating backbuffer and swapping.
  // But we release frontbuffer by telling UI to release its handle on it.
  if (!frontbuffer_suggested_allocation_)
    helper_->Suspend();
}

bool IOSurfaceImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = io_surface_handle_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

bool IOSurfaceImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  glFlush();

  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = io_surface_handle_;
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  helper_->SetScheduled(false);
  return true;
}

std::string IOSurfaceImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached ";
  extensions += "GL_CHROMIUM_post_sub_buffer";
  return extensions;
}

gfx::Size IOSurfaceImageTransportSurface::GetSize() {
  return size_;
}

void IOSurfaceImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void IOSurfaceImageTransportSurface::OnPostSubBufferACK() {
  helper_->SetScheduled(true);
}

void IOSurfaceImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_handle,
    TransportDIB::Handle /* shm_handle */) {
  NOTREACHED();
}

void IOSurfaceImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void IOSurfaceImageTransportSurface::OnResize(gfx::Size size) {
  // Caching |context_| from OnMakeCurrent. It should still be current.
  DCHECK(context_->IsCurrent(this));

  size_ = size;

  CreateIOSurface();
}

void IOSurfaceImageTransportSurface::UnrefIOSurface() {
  DCHECK(context_->IsCurrent(this));

  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    fbo_id_ = 0;
  }

  if (texture_id_) {
    glDeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }

  io_surface_.reset();
  io_surface_handle_ = 0;
}

void IOSurfaceImageTransportSurface::CreateIOSurface() {
  GLint previous_texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &previous_texture_id);

  UnrefIOSurface();

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);

  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();

  glGenTextures(1, &texture_id_);

  // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on
  // Mac OS X and is required for IOSurface interoperability.
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glBindTexture(target, texture_id_);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            target,
                            texture_id_,
                            0);

  // Allocate a new IOSurface, which is the GPU resource that can be
  // shared across processes.
  base::mac::ScopedCFTypeRef<CFMutableDictionaryRef> properties;
  properties.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceWidth(),
                  size_.width());
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceHeight(),
                  size_.height());
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceBytesPerElement(), 4);
  AddBooleanValue(properties,
                  io_surface_support->GetKIOSurfaceIsGlobal(), true);
  // I believe we should be able to unreference the IOSurfaces without
  // synchronizing with the browser process because they are
  // ultimately reference counted by the operating system.
  io_surface_.reset(io_surface_support->IOSurfaceCreate(properties));

  // Don't think we need to identify a plane.
  GLuint plane = 0;
  CGLError cglerror =
      io_surface_support->CGLTexImageIOSurface2D(
          static_cast<CGLContextObj>(context_->GetHandle()),
          target,
          GL_RGBA,
          size_.width(),
          size_.height(),
          GL_BGRA,
          GL_UNSIGNED_INT_8_8_8_8_REV,
          io_surface_.get(),
          plane);
  if (cglerror != kCGLNoError) {
    DLOG(ERROR) << "CGLTexImageIOSurface2D: " << cglerror;
    UnrefIOSurface();
    return;
  }

  io_surface_handle_ = io_surface_support->IOSurfaceGetID(io_surface_);
  glFlush();

  glBindTexture(target, previous_texture_id);
  // The FBO remains bound for this GL context.
}

}  // namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& surface_handle) {
  scoped_refptr<gfx::GLSurface> surface;
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();

  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
    case gfx::kGLImplementationAppleGL:
      if (!io_surface_support) {
        DLOG(WARNING) << "No IOSurface support";
        return NULL;
      } else {
        surface = new IOSurfaceImageTransportSurface(
            manager, stub, surface_handle.handle);
      }
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
