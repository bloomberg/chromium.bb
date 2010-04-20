// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_OSMESA_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_OSMESA_H_

// Ensure that gl_utils.h is included before any GL headers.
#include "gpu/command_buffer/service/gl_utils.h"

#include "base/scoped_ptr.h"
#include "gfx/size.h"
#include "gpu/command_buffer/service/gl_context.h"

namespace gpu {

// Encapsulates an OSMesa OpenGL context that uses software rendering.
class OSMesaGLContext : public GLContext {
 public:
  OSMesaGLContext();
  virtual ~OSMesaGLContext();

  // Initialize an OSMesa GL context with the default 1 x 1 initial size.
  bool Initialize(void* shared_handle);

  // Implement GLContext.
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual void SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

  // Resize the back buffer, preserving the old content. Does nothing if the
  // size is unchanged.
  void Resize(const gfx::Size& new_size);

  const void* buffer() const {
    return buffer_.get();
  }

 protected:
  bool InitializeCommon();

 private:
#if !defined(UNIT_TEST)
  gfx::Size size_;
  scoped_array<int32> buffer_;
  OSMesaContext context_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OSMesaGLContext);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_CONTEXT_OSMESA_H_
