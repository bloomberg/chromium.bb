// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GL/glew.h>
#include <GL/osmew.h>

#include <algorithm>

#include "app/gfx/gl/gl_context_osmesa.h"

namespace gfx {

OSMesaGLContext::OSMesaGLContext()
#if !defined(UNIT_TEST)
    : context_(NULL)
#endif
{
}

OSMesaGLContext::~OSMesaGLContext() {
}

bool OSMesaGLContext::Initialize(void* shared_handle) {
#if !defined(UNIT_TEST)
  DCHECK(!context_);

  size_ = gfx::Size(1, 1);
  buffer_.reset(new int32[1]);

  context_ = OSMesaCreateContext(GL_RGBA,
                                 static_cast<OSMesaContext>(shared_handle));
  return context_ != NULL;
#else
  return true;
#endif
}

void OSMesaGLContext::Destroy() {
#if !defined(UNIT_TEST)
  if (context_) {
    OSMesaDestroyContext(static_cast<OSMesaContext>(context_));
    context_ = NULL;
  }
#endif
}

bool OSMesaGLContext::MakeCurrent() {
#if !defined(UNIT_TEST)
  DCHECK(context_);
  return OSMesaMakeCurrent(static_cast<OSMesaContext>(context_),
                           buffer_.get(),
                           GL_UNSIGNED_BYTE,
                           size_.width(), size_.height()) == GL_TRUE;
#endif
  return true;
}

bool OSMesaGLContext::IsCurrent() {
#if !defined(UNIT_TEST)
  DCHECK(context_);
  return context_ == OSMesaGetCurrentContext();
#endif
  return true;
}

bool OSMesaGLContext::IsOffscreen() {
  return true;
}

void OSMesaGLContext::SwapBuffers() {
  NOTREACHED() << "Should not call SwapBuffers on an OSMesaGLContext.";
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
