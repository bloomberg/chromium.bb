// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/accelerated_surface_mac.h"

#include "base/logging.h"
#include "chrome/common/io_surface_support_mac.h"
#include "gfx/rect.h"

AcceleratedSurface::AcceleratedSurface()
    : gl_context_(NULL),
      pbuffer_(NULL),
      surface_width_(0),
      surface_height_(0),
      texture_(0),
      fbo_(0),
      depth_stencil_renderbuffer_(0),
      bound_fbo_(0),
      bound_renderbuffer_(0) {
}

bool AcceleratedSurface::Initialize() {
  // Create a 1x1 pbuffer and associated context to bootstrap things
  static const CGLPixelFormatAttribute attribs[] = {
    (CGLPixelFormatAttribute) kCGLPFAPBuffer,
    (CGLPixelFormatAttribute) 0
  };
  CGLPixelFormatObj pixel_format;
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(attribs,
                           &pixel_format,
                           &num_pixel_formats) != kCGLNoError) {
    DLOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (!pixel_format) {
    return false;
  }
  CGLContextObj context;
  CGLError res = CGLCreateContext(pixel_format, 0, &context);
  CGLDestroyPixelFormat(pixel_format);
  if (res != kCGLNoError) {
    DLOG(ERROR) << "Error creating context.";
    return false;
  }
  CGLPBufferObj pbuffer;
  if (CGLCreatePBuffer(1, 1,
                       GL_TEXTURE_2D, GL_RGBA,
                       0, &pbuffer) != kCGLNoError) {
    CGLDestroyContext(context);
    DLOG(ERROR) << "Error creating pbuffer.";
    return false;
  }
  if (CGLSetPBuffer(context, pbuffer, 0, 0, 0) != kCGLNoError) {
    CGLDestroyContext(context);
    CGLDestroyPBuffer(pbuffer);
    DLOG(ERROR) << "Error attaching pbuffer to context.";
    return false;
  }
  gl_context_ = context;
  pbuffer_ = pbuffer;
  // Now we're ready to handle SetWindowSize calls, which will
  // allocate and/or reallocate the IOSurface and associated offscreen
  // OpenGL structures for rendering.
  return true;
}

void AcceleratedSurface::Destroy() {
  // Release the old TransportDIB in the browser.
  if (dib_free_callback_.get() && transport_dib_.get()) {
    dib_free_callback_->Run(transport_dib_->id());
  }
  transport_dib_.reset();
  if (gl_context_)
    CGLDestroyContext(gl_context_);
  if (pbuffer_)
    CGLDestroyPBuffer(pbuffer_);
}

// Call after making changes to the surface which require a visual update.
// Makes the rendering show up in other processes.
void AcceleratedSurface::SwapBuffers() {
  if (bound_fbo_ != fbo_) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  }
  if (io_surface_.get() != NULL) {
    // Bind and unbind the framebuffer to make changes to the
    // IOSurface show up in the other process.
    glFlush();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  } else if (transport_dib_.get() != NULL) {
    // Pre-Mac OS X 10.6, fetch the rendered image from the FBO and copy it
    // into the TransportDIB.
    // TODO(dspringer): There are a couple of options that can speed this up.
    // First is to use async reads into a PBO, second is to use SPI that
    // allows many tasks to access the same CGSSurface.
    void* pixel_memory = transport_dib_->memory();
    if (pixel_memory) {
      // Note that glReadPixels does an implicit glFlush().
      glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
      glReadPixels(0,
                   0,
                   surface_width_,
                   surface_height_,
                   GL_BGRA,  // This pixel format should have no conversion.
                   GL_UNSIGNED_INT_8_8_8_8_REV,
                   pixel_memory);
    }
  }
  if (bound_fbo_ != fbo_) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
  }
}

static void AddBooleanValue(CFMutableDictionaryRef dictionary,
                            const CFStringRef key,
                            bool value) {
  CFDictionaryAddValue(dictionary, key,
                       (value ? kCFBooleanTrue : kCFBooleanFalse));
}

static void AddIntegerValue(CFMutableDictionaryRef dictionary,
                            const CFStringRef key,
                            int32 value) {
  CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
  CFDictionaryAddValue(dictionary, key, number);
}

void AcceleratedSurface::AllocateRenderBuffers(GLenum target,
                                               int32 width, int32 height) {
  if (!texture_) {
    // Generate the texture object.
    glGenTextures(1, &texture_);
    glBindTexture(target, texture_);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Generate and bind the framebuffer object.
    glGenFramebuffersEXT(1, &fbo_);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
    bound_fbo_ = fbo_;
    // Generate (but don't bind) the depth buffer -- we don't need
    // this bound in order to do offscreen rendering.
    glGenRenderbuffersEXT(1, &depth_stencil_renderbuffer_);
  }

  // Reallocate the depth buffer.
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_stencil_renderbuffer_);
  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                           GL_DEPTH24_STENCIL8_EXT,
                           width,
                           height);

  // Unbind the renderbuffers.
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, bound_renderbuffer_);

  // Make sure that subsequent set-up code affects the render texture.
  glBindTexture(target, texture_);
}

bool AcceleratedSurface::SetupFrameBufferObject(GLenum target) {
  if (bound_fbo_ != fbo_) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  }
  GLenum fbo_status;
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            target,
                            texture_,
                            0);
  fbo_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (fbo_status == GL_FRAMEBUFFER_COMPLETE_EXT) {
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 depth_stencil_renderbuffer_);
    fbo_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  }
  // Attach the depth and stencil buffer.
  if (fbo_status == GL_FRAMEBUFFER_COMPLETE_EXT) {
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 0x8D20, // GL_STENCIL_ATTACHMENT,
                                 GL_RENDERBUFFER_EXT,
                                 depth_stencil_renderbuffer_);
    fbo_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  }
  if (bound_fbo_ != fbo_) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
  }
  return fbo_status == GL_FRAMEBUFFER_COMPLETE_EXT;
}

bool AcceleratedSurface::MakeCurrent() {
  if (CGLGetCurrentContext() != gl_context_) {
    if (CGLSetCurrentContext(gl_context_) != kCGLNoError) {
      DLOG(ERROR) << "Unable to make gl context current.";
      return false;
    }
  }
  return true;
}

void AcceleratedSurface::Clear(const gfx::Rect& rect) {
  glClearColor(1.0, 1.0, 1.0, 1.0);
  glViewport(0, 0, rect.width(), rect.height());
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, rect.width(), 0, rect.height(), -1, 1);
  glClear(GL_COLOR_BUFFER_BIT);
}

uint64 AcceleratedSurface::SetSurfaceSize(int32 width, int32 height) {
  if (surface_width_ == width && surface_height_ == height) {
    // Return 0 to indicate to the caller that no new backing store
    // allocation occurred.
    return 0;
  }

  IOSurfaceSupport* io_surface_support = IOSurfaceSupport::Initialize();
  if (!io_surface_support)
    return 0;  // Caller can try using SetWindowSizeForTransportDIB().

  if (!MakeCurrent())
    return 0;

  // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on
  // Mac OS X and is required for IOSurface interoperability.
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  AllocateRenderBuffers(target, width, height);

  // Allocate a new IOSurface, which is the GPU resource that can be
  // shared across processes.
  scoped_cftyperef<CFMutableDictionaryRef> properties;
  properties.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceWidth(), width);
  AddIntegerValue(properties,
                  io_surface_support->GetKIOSurfaceHeight(), height);
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
  io_surface_support->CGLTexImageIOSurface2D(gl_context_,
                                             target,
                                             GL_RGBA,
                                             width,
                                             height,
                                             GL_BGRA,
                                             GL_UNSIGNED_INT_8_8_8_8_REV,
                                             io_surface_.get(),
                                             plane);
  // Set up the frame buffer object.
  SetupFrameBufferObject(target);
  surface_width_ = width;
  surface_height_ = height;

  // Now send back an identifier for the IOSurface. We originally
  // intended to send back a mach port from IOSurfaceCreateMachPort
  // but it looks like Chrome IPC would need to be modified to
  // properly send mach ports between processes. For the time being we
  // make our IOSurfaces global and send back their identifiers. On
  // the browser process side the identifier is reconstituted into an
  // IOSurface for on-screen rendering.
  return io_surface_support->IOSurfaceGetID(io_surface_);
}

TransportDIB::Handle AcceleratedSurface::SetTransportDIBSize(
    int32 width, int32 height) {
  if (surface_width_ == width && surface_height_ == height) {
    // Return an invalid handle to indicate to the caller that no new backing
    // store allocation occurred.
    return TransportDIB::DefaultHandleValue();
  }
  surface_width_ = width;
  surface_height_ = height;

  // Release the old TransportDIB in the browser.
  if (dib_free_callback_.get() && transport_dib_.get()) {
    dib_free_callback_->Run(transport_dib_->id());
  }
  transport_dib_.reset();

  // Ask the renderer to create a TransportDIB.
  size_t dib_size = width * 4 * height;  // 4 bytes per pixel.
  TransportDIB::Handle dib_handle;
  if (dib_alloc_callback_.get()) {
    dib_alloc_callback_->Run(dib_size, &dib_handle);
  }
  if (!TransportDIB::is_valid(dib_handle)) {
    // If the allocator fails, it means the DIB was not created in the browser,
    // so there is no need to run the deallocator here.
    return TransportDIB::DefaultHandleValue();
  }
  transport_dib_.reset(TransportDIB::Map(dib_handle));
  if (transport_dib_.get() == NULL) {
    // TODO(dspringer): if the Map() fails, should the deallocator be run so
    // that the DIB is deallocated in the browser?
    return TransportDIB::DefaultHandleValue();
  }

  // Set up the render buffers and reserve enough space on the card for the
  // framebuffer texture.
  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  AllocateRenderBuffers(target, width, height);
  glTexImage2D(target,
               0,  // mipmap level 0
               GL_RGBA8,  // internal pixel format
               width,
               height,
               0,  // 0 border
               GL_BGRA,  // Used for consistency
               GL_UNSIGNED_INT_8_8_8_8_REV,
               NULL);  // No data, just reserve room on the card.
  SetupFrameBufferObject(target);
  return transport_dib_->handle();
}

void AcceleratedSurface::SetTransportDIBAllocAndFree(
    Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
    Callback1<TransportDIB::Id>::Type* deallocator) {
  dib_alloc_callback_.reset(allocator);
  dib_free_callback_.reset(deallocator);
}
