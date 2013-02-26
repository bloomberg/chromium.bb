// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/texture_image_transport_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_cgl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/surface/io_surface_support_mac.h"

namespace content {
namespace {

// IOSurface dimensions will be rounded up to a multiple of this value in order
// to reduce memory thrashing during resize. This must be a power of 2.
const uint32 kIOSurfaceDimensionRoundup = 64;

int RoundUpSurfaceDimension(int number) {
  DCHECK(number >= 0);
  // Cast into unsigned space for portable bitwise ops.
  uint32 unsigned_number = static_cast<uint32>(number);
  uint32 roundup_sub_1 = kIOSurfaceDimensionRoundup - 1;
  unsigned_number = (unsigned_number + roundup_sub_1) & ~roundup_sub_1;
  return static_cast<int>(unsigned_number);
}

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
  virtual bool DeferDraws() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual bool SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~IOSurfaceImageTransportSurface() OVERRIDE;

  void AdjustBufferAllocation();
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
  gfx::Size rounded_size_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  // Whether a SwapBuffers is pending.
  bool is_swap_buffers_pending_;

  // Whether we unscheduled command buffer because of pending SwapBuffers.
  bool did_unschedule_;

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
      made_current_(false),
      is_swap_buffers_pending_(false),
      did_unschedule_(false) {
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
  UnrefIOSurface();

  helper_->Destroy();
  NoOpGLSurfaceCGL::Destroy();
}

bool IOSurfaceImageTransportSurface::DeferDraws() {
  // The command buffer hit a draw/clear command that could clobber the
  // IOSurface in use by an earlier SwapBuffers. If a Swap is pending, abort
  // processing of the command by returning true and unschedule until the Swap
  // Ack arrives.
  if(did_unschedule_)
    return true;  // Still unscheduled, so just return true.
  if (is_swap_buffers_pending_) {
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
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

bool IOSurfaceImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  if (backbuffer_suggested_allocation_ == allocation)
    return true;
  backbuffer_suggested_allocation_ = allocation;
  AdjustBufferAllocation();
  return true;
}

void IOSurfaceImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;
  AdjustBufferAllocation();
}

void IOSurfaceImageTransportSurface::AdjustBufferAllocation() {
  // On mac, the frontbuffer and backbuffer are the same buffer. The buffer is
  // free'd when both the browser and gpu processes have Unref'd the IOSurface.
  if (!backbuffer_suggested_allocation_ &&
      !frontbuffer_suggested_allocation_ &&
      io_surface_.get()) {
    UnrefIOSurface();
    helper_->Suspend();
  } else if (backbuffer_suggested_allocation_ && !io_surface_.get()) {
    CreateIOSurface();
  }
}

bool IOSurfaceImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = io_surface_handle_;
  params.size = GetSize();
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
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
  params.surface_size = GetSize();
  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
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

void IOSurfaceImageTransportSurface::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& /* params */) {
  DCHECK(is_swap_buffers_pending_);
  is_swap_buffers_pending_ = false;
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void IOSurfaceImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void IOSurfaceImageTransportSurface::OnResize(gfx::Size size) {
  // This trace event is used in gpu_feature_browsertest.cc - the test will need
  // to be updated if this event is changed or moved.
  TRACE_EVENT2("gpu", "IOSurfaceImageTransportSurface::OnResize",
               "old_width", size_.width(), "new_width", size.width());
  // Caching |context_| from OnMakeCurrent. It should still be current.
  DCHECK(context_->IsCurrent(this));

  size_ = size;

  CreateIOSurface();
}

void IOSurfaceImageTransportSurface::UnrefIOSurface() {
  // If we have resources to destroy, then make sure that we have a current
  // context which we can use to delete the resources.
  if (context_ || fbo_id_ || texture_id_) {
    DCHECK(gfx::GLContext::GetCurrent() == context_);
    DCHECK(context_->IsCurrent(this));
    DCHECK(CGLGetCurrentContext());
  }

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
  gfx::Size new_rounded_size(RoundUpSurfaceDimension(size_.width()),
                             RoundUpSurfaceDimension(size_.height()));

  // Only recreate surface when the rounded up size has changed.
  if (io_surface_.get() && new_rounded_size == rounded_size_)
    return;

  // This trace event is used in gpu_feature_browsertest.cc - the test will need
  // to be updated if this event is changed or moved.
  TRACE_EVENT2("gpu", "IOSurfaceImageTransportSurface::CreateIOSurface",
               "width", new_rounded_size.width(),
               "height", new_rounded_size.height());

  rounded_size_ = new_rounded_size;

  GLint previous_texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &previous_texture_id);

  // Free the old IO Surface first to reduce memory fragmentation.
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
                  rounded_size_.width());
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceHeight(),
                  rounded_size_.height());
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceBytesPerElement(), 4);
  AddBooleanValue(properties,
                  io_surface_support->GetKIOSurfaceIsGlobal(), true);
  // I believe we should be able to unreference the IOSurfaces without
  // synchronizing with the browser process because they are
  // ultimately reference counted by the operating system.
  io_surface_.reset(io_surface_support->IOSurfaceCreate(properties));
  io_surface_handle_ = io_surface_support->IOSurfaceGetID(io_surface_);

  // Don't think we need to identify a plane.
  GLuint plane = 0;
  CGLError cglerror =
      io_surface_support->CGLTexImageIOSurface2D(
          static_cast<CGLContextObj>(context_->GetHandle()),
          target,
          GL_RGBA,
          rounded_size_.width(),
          rounded_size_.height(),
          GL_BGRA,
          GL_UNSIGNED_INT_8_8_8_8_REV,
          io_surface_.get(),
          plane);
  if (cglerror != kCGLNoError) {
    DLOG(ERROR) << "CGLTexImageIOSurface2D: " << cglerror;
    UnrefIOSurface();
    return;
  }

  glFlush();

  glBindTexture(target, previous_texture_id);
  // The FBO remains bound for this GL context.
}

// A subclass of GLSurfaceOSMesa that doesn't print an error message when
// SwapBuffers() is called.
class DRTSurfaceOSMesa : public gfx::GLSurfaceOSMesa {
 public:
  // Size doesn't matter, the surface is resized to the right size later.
  DRTSurfaceOSMesa() : GLSurfaceOSMesa(GL_RGBA, gfx::Size(1, 1)) {}

  // Implement a subset of GLSurface.
  virtual bool SwapBuffers() OVERRIDE;

 private:
  virtual ~DRTSurfaceOSMesa() {}
  DISALLOW_COPY_AND_ASSIGN(DRTSurfaceOSMesa);
};

bool DRTSurfaceOSMesa::SwapBuffers() {
  return true;
}

bool g_allow_os_mesa = false;

}  // namespace

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& surface_handle) {
  scoped_refptr<gfx::GLSurface> surface;
  if (surface_handle.transport_type == gfx::TEXTURE_TRANSPORT) {
     surface = new TextureImageTransportSurface(manager, stub, surface_handle);
  } else {
    DCHECK(surface_handle.transport_type == gfx::NATIVE_TRANSPORT);
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
        // Content shell in DRT mode spins up a gpu process which needs an
        // image transport surface, but that surface isn't used to read pixel
        // baselines. So this is mostly a dummy surface.
        if (g_allow_os_mesa) {
          surface = new DRTSurfaceOSMesa();
        } else {
          NOTREACHED();
          return NULL;
        }
    }
  }
  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

// static
void ImageTransportSurface::SetAllowOSMesaForTesting(bool allow) {
  g_allow_os_mesa = allow;
}

}  // namespace content
