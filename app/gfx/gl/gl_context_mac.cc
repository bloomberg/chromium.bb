// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the ViewGLContext and PbufferGLContext classes.

#include <GL/glew.h>
#include <GL/osmew.h>
#include <OpenGL/OpenGL.h>

#include "app/surface/accelerated_surface_mac.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_context_osmesa.h"

namespace gfx {

typedef CGLContextObj GLContextHandle;
typedef CGLPBufferObj PbufferHandle;

// This class is a wrapper around a GL context used for offscreen rendering.
// It is initially backed by a 1x1 pbuffer. Use it to create an FBO to do useful
// rendering.
class PbufferGLContext : public GLContext {
 public:
  PbufferGLContext()
      : context_(NULL),
        pbuffer_(NULL) {
  }

  // Initializes the GL context.
  bool Initialize(void* shared_handle);

  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

 private:
  GLContextHandle context_;
  PbufferHandle pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLContext);
};

static bool InitializeOneOff() {
  static bool initialized = false;
  if (initialized)
    return true;

  osmewInit();
  initialized = true;
  return true;
}

bool PbufferGLContext::Initialize(void* shared_handle) {
  // Create a 1x1 pbuffer and associated context to bootstrap things.
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
    Destroy();
    return false;
  }
  if (!pixel_format) {
    return false;
  }
  CGLError res = CGLCreateContext(pixel_format,
                                  static_cast<GLContextHandle>(shared_handle),
                                  &context_);
  CGLDestroyPixelFormat(pixel_format);
  if (res != kCGLNoError) {
    DLOG(ERROR) << "Error creating context.";
    Destroy();
    return false;
  }
  if (CGLCreatePBuffer(1, 1,
                       GL_TEXTURE_2D, GL_RGBA,
                       0, &pbuffer_) != kCGLNoError) {
    DLOG(ERROR) << "Error creating pbuffer.";
    Destroy();
    return false;
  }
  if (CGLSetPBuffer(context_, pbuffer_, 0, 0, 0) != kCGLNoError) {
    DLOG(ERROR) << "Error attaching pbuffer to context.";
    Destroy();
    return false;
  }

  if (!MakeCurrent()) {
    Destroy();
    DLOG(ERROR) << "Couldn't make context current for initialization.";
    return false;
  }

  if (!InitializeGLEW()) {
    Destroy();
    return false;
  }

  if (!InitializeCommon()) {
    Destroy();
    return false;
  }

  return true;
}

void PbufferGLContext::Destroy() {
  if (context_) {
    CGLDestroyContext(context_);
    context_ = NULL;
  }

  if (pbuffer_) {
    CGLDestroyPBuffer(pbuffer_);
    pbuffer_ = NULL;
  }
}

bool PbufferGLContext::MakeCurrent() {
  if (!IsCurrent()) {
    if (CGLSetCurrentContext(context_) != kCGLNoError) {
      DLOG(ERROR) << "Unable to make gl context current.";
      return false;
    }
  }

  return true;
}

bool PbufferGLContext::IsCurrent() {
  return CGLGetCurrentContext() == context_;
}

bool PbufferGLContext::IsOffscreen() {
  return true;
}

void PbufferGLContext::SwapBuffers() {
  NOTREACHED() << "Cannot call SwapBuffers on a PbufferGLContext.";
}

gfx::Size PbufferGLContext::GetSize() {
  NOTREACHED() << "Should not be requesting size of a PbufferGLContext.";
  return gfx::Size(1, 1);
}

void* PbufferGLContext::GetHandle() {
  return context_;
}

GLContext* GLContext::CreateOffscreenGLContext(void* shared_handle) {
  if (!InitializeOneOff())
    return NULL;

  if (OSMesaCreateContext) {
    scoped_ptr<OSMesaGLContext> context(new OSMesaGLContext);

    if (!context->Initialize(shared_handle))
      return NULL;

    return context.release();
  } else {
    scoped_ptr<PbufferGLContext> context(new PbufferGLContext);
    if (!context->Initialize(shared_handle))
      return NULL;

    return context.release();
  }
}

}  // namespace gfx
