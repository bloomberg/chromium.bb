// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/image_transport_surface.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_cgl.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/surface/io_surface_support_mac.h"

namespace {

// We are backed by an Pbuffer offscreen surface for the purposes of creating a
// context, but use FBOs to render to texture backed IOSurface
class IOSurfaceImageTransportSurface : public gfx::PbufferGLSurfaceCGL,
                                       public ImageTransportSurface {
 public:
  IOSurfaceImageTransportSurface(GpuChannelManager* manager,
                                 int32 render_view_id,
                                 int32 renderer_id,
                                 int32 command_buffer_id,
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

 protected:
  // ImageTransportSurface implementation
  virtual void OnNewSurfaceACK(uint64 surface_id,
                               TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~IOSurfaceImageTransportSurface() OVERRIDE;

  uint32 fbo_id_;
  GLuint texture_id_;

  base::mac::ScopedCFTypeRef<CFTypeRef> io_surface_;

  // The id of |io_surface_| or 0 if that's NULL.
  uint64 io_surface_id_;

  // Weak pointer to the context that this was last made current to.
  gfx::GLContext* context_;

  gfx::Size size_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceImageTransportSurface);
};

// We are backed by an Pbuffer offscreen surface for the purposes of creating a
// context, but use FBOs to render offscreen.
class TransportDIBImageTransportSurface : public gfx::PbufferGLSurfaceCGL,
                                          public ImageTransportSurface {
 public:
  TransportDIBImageTransportSurface(GpuChannelManager* manager,
                                    int32 render_view_id,
                                    int32 renderer_id,
                                    int32 command_buffer_id,
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

 protected:
  // ImageTransportSurface implementation
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnNewSurfaceACK(uint64 surface_id,
                               TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

 private:
  virtual ~TransportDIBImageTransportSurface() OVERRIDE;

  uint32 fbo_id_;
  GLuint render_buffer_id_;

  scoped_ptr<TransportDIB> shared_mem_;

  gfx::Size size_;

  static uint32 next_id_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(TransportDIBImageTransportSurface);
};

uint32 TransportDIBImageTransportSurface::next_id_ = 1;

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
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id,
    gfx::PluginWindowHandle handle)
        : gfx::PbufferGLSurfaceCGL(gfx::Size(1, 1)),
          fbo_id_(0),
          texture_id_(0),
          io_surface_id_(0),
          context_(NULL),
          made_current_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
                                         handle));

}

IOSurfaceImageTransportSurface::~IOSurfaceImageTransportSurface() {
  Destroy();
}

bool IOSurfaceImageTransportSurface::Initialize() {
  // Only support IOSurfaces if the GL implementation is the native desktop GL.
  // IO surfaces will not work with, for example, OSMesa software renderer
  // GL contexts.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL)
    return false;

  if (!helper_->Initialize())
    return false;
  return PbufferGLSurfaceCGL::Initialize();
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
  PbufferGLSurfaceCGL::Destroy();
}

bool IOSurfaceImageTransportSurface::IsOffscreen() {
  return false;
}

bool IOSurfaceImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  context_ = context;

  if (made_current_)
    return true;

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);
  OnResize(gfx::Size(1, 1));

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "Framebuffer incomplete.";
    return false;
  }

  made_current_ = true;
  return true;
}

unsigned int IOSurfaceImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

bool IOSurfaceImageTransportSurface::SwapBuffers() {
  glFlush();

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = io_surface_id_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

bool IOSurfaceImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  NOTREACHED();
  return false;
}

std::string IOSurfaceImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached";
  return extensions;
}

gfx::Size IOSurfaceImageTransportSurface::GetSize() {
  return size_;
}

void IOSurfaceImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void IOSurfaceImageTransportSurface::OnPostSubBufferACK() {
  NOTREACHED();
}

void IOSurfaceImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id,
    TransportDIB::Handle /* shm_handle */) {
  DCHECK_EQ(io_surface_id_, surface_id);
  helper_->SetScheduled(true);
}

void IOSurfaceImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void IOSurfaceImageTransportSurface::OnResize(gfx::Size size) {
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();

  // Caching |context_| from OnMakeCurrent. It should still be current.
  DCHECK(context_->IsCurrent(this));

  size_ = size;

  if (texture_id_) {
    glDeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }

  glGenTextures(1, &texture_id_);

  GLint previous_texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &previous_texture_id);

  // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on
  // Mac OS X and is required for IOSurface interoperability.
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glBindTexture(target, texture_id_);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  GLint previous_fbo_id = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo_id);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);

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

  io_surface_id_ = io_surface_support->IOSurfaceGetID(io_surface_);
  glFlush();

  glBindTexture(target, previous_texture_id);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previous_fbo_id);

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size_.width();
  params.height = size_.height();
  params.surface_id = io_surface_id_;
  params.create_transport_dib = false;
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
}

TransportDIBImageTransportSurface::TransportDIBImageTransportSurface(
    GpuChannelManager* manager,
    int32 render_view_id,
    int32 renderer_id,
    int32 command_buffer_id,
    gfx::PluginWindowHandle handle)
        : gfx::PbufferGLSurfaceCGL(gfx::Size(1, 1)),
          fbo_id_(0),
          render_buffer_id_(0),
          made_current_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         render_view_id,
                                         renderer_id,
                                         command_buffer_id,
                                         handle));

}

TransportDIBImageTransportSurface::~TransportDIBImageTransportSurface() {
  Destroy();
}

bool TransportDIBImageTransportSurface::Initialize() {
  if (!helper_->Initialize())
    return false;
  return PbufferGLSurfaceCGL::Initialize();
}

void TransportDIBImageTransportSurface::Destroy() {
  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    fbo_id_ = 0;
  }

  if (render_buffer_id_) {
    glDeleteRenderbuffersEXT(1, &render_buffer_id_);
    render_buffer_id_ = 0;
  }

  helper_->Destroy();
  PbufferGLSurfaceCGL::Destroy();
}

bool TransportDIBImageTransportSurface::IsOffscreen() {
  return false;
}

bool TransportDIBImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (made_current_)
    return true;

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);
  OnResize(gfx::Size(1, 1));

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "Framebuffer incomplete.";
    return false;
  }

  made_current_ = true;
  return true;
}

unsigned int TransportDIBImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

bool TransportDIBImageTransportSurface::SwapBuffers() {
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));

  GLint previous_fbo_id = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previous_fbo_id);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);

  GLint current_alignment = 0;
  glGetIntegerv(GL_PACK_ALIGNMENT, &current_alignment);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glReadPixels(0, 0,
               size_.width(), size_.height(),
               GL_BGRA,  // This pixel format should have no conversion.
               GL_UNSIGNED_INT_8_8_8_8_REV,
               shared_mem_->memory());
  glPixelStorei(GL_PACK_ALIGNMENT, current_alignment);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previous_fbo_id);

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_id = next_id_;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  helper_->SetScheduled(false);
  return true;
}

bool TransportDIBImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  NOTREACHED();
  return false;
}

std::string TransportDIBImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached";
  return extensions;
}

gfx::Size TransportDIBImageTransportSurface::GetSize() {
  return size_;
}

void TransportDIBImageTransportSurface::OnBuffersSwappedACK() {
  helper_->SetScheduled(true);
}

void TransportDIBImageTransportSurface::OnPostSubBufferACK() {
  NOTREACHED();
}

void TransportDIBImageTransportSurface::OnNewSurfaceACK(
    uint64 surface_id,
    TransportDIB::Handle shm_handle) {
  helper_->SetScheduled(true);

  shared_mem_.reset(TransportDIB::Map(shm_handle));
  DCHECK_NE(shared_mem_.get(), static_cast<void*>(NULL));
}

void TransportDIBImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void TransportDIBImageTransportSurface::OnResize(gfx::Size size) {
  size_ = size;

  if (!render_buffer_id_)
    glGenRenderbuffersEXT(1, &render_buffer_id_);

  GLint previous_fbo_id = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &previous_fbo_id);

  GLint previous_renderbuffer_id = 0;
  glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT, &previous_renderbuffer_id);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffer_id_);

  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                           GL_RGBA,
                           size_.width(), size_.height());
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                               GL_COLOR_ATTACHMENT0_EXT,
                               GL_RENDERBUFFER_EXT,
                               render_buffer_id_);

  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, previous_renderbuffer_id);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, previous_fbo_id);

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size_.width();
  params.height = size_.height();
  params.surface_id = next_id_++;
  params.create_transport_dib = true;
  helper_->SendAcceleratedSurfaceNew(params);

  helper_->SetScheduled(false);
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
  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();

  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationDesktopGL:
      if (!io_surface_support) {
        surface = new TransportDIBImageTransportSurface(manager,
                                                        render_view_id,
                                                        renderer_id,
                                                        command_buffer_id,
                                                        handle);
      } else {
        surface = new IOSurfaceImageTransportSurface(manager,
                                                     render_view_id,
                                                     renderer_id,
                                                     command_buffer_id,
                                                     handle);
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
