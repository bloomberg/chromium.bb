// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GL/osmesa.h>

#include <algorithm>

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_osmesa.h"
#include "base/logging.h"

namespace gfx {

OSMesaGLContext::OSMesaGLContext() : context_(NULL)
{
}

OSMesaGLContext::~OSMesaGLContext() {
}

bool OSMesaGLContext::Initialize(GLuint format, GLContext* shared_context) {
  DCHECK(!context_);

  size_ = gfx::Size(1, 1);
  buffer_.reset(new int32[1]);

  OSMesaContext shared_handle = NULL;
  if (shared_context)
    shared_handle = static_cast<OSMesaContext>(shared_context->GetHandle());

  context_ = OSMesaCreateContextExt(format,
                                    24,  // depth bits
                                    8,  // stencil bits
                                    0,  // accum bits
                                    shared_handle);
  if (!context_) {
    LOG(ERROR) << "OSMesaCreateContextExt failed.";
    return false;
  }

  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    Destroy();
    return false;
  }

  // Row 0 is at the top.
  OSMesaPixelStore(OSMESA_Y_UP, 0);

  if (!InitializeCommon()) {
    LOG(ERROR) << "GLContext::InitializeCommon failed.";
    Destroy();
    return false;
  }

  return true;
}

void OSMesaGLContext::Destroy() {
  if (context_) {
    OSMesaDestroyContext(static_cast<OSMesaContext>(context_));
    context_ = NULL;
  }
  buffer_.reset();
  size_ = gfx::Size();
}

bool OSMesaGLContext::MakeCurrent() {
  DCHECK(context_);
  return OSMesaMakeCurrent(static_cast<OSMesaContext>(context_),
                           buffer_.get(),
                           GL_UNSIGNED_BYTE,
                           size_.width(), size_.height()) == GL_TRUE;
  return true;
}

bool OSMesaGLContext::IsCurrent() {
  DCHECK(context_);
  return context_ == OSMesaGetCurrentContext();
}

bool OSMesaGLContext::IsOffscreen() {
  return true;
}

bool OSMesaGLContext::SwapBuffers() {
  NOTREACHED() << "Should not call SwapBuffers on an OSMesaGLContext.";
  return false;
}

gfx::Size OSMesaGLContext::GetSize() {
  return size_;
}

void* OSMesaGLContext::GetHandle() {
  return context_;
}

void OSMesaGLContext::Resize(const gfx::Size& new_size) {
  if (new_size == size_)
    return;

  // Allocate a new back buffer.
  scoped_array<int32> new_buffer(new int32[new_size.GetArea()]);
  memset(new_buffer.get(), 0, new_size.GetArea() * sizeof(new_buffer[0]));

  // Copy the current back buffer into the new buffer.
  int copy_width = std::min(size_.width(), new_size.width());
  int copy_height = std::min(size_.height(), new_size.height());
  for (int y = 0; y < copy_height; ++y) {
    for (int x = 0; x < copy_width; ++x) {
      new_buffer[y * new_size.width() + x] = buffer_[y * size_.width() + x];
    }
  }

  buffer_.reset(new_buffer.release());
  size_ = new_size;

  // If this context is current, need to call MakeCurrent again so OSMesa uses
  // the new buffer.
  if (IsCurrent())
    MakeCurrent();
}

}  // namespace gfx
