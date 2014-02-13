// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_
#define GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "gles2_impl_export.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"

namespace gfx {
class Size;
}

#if defined(OS_ANDROID)
namespace gfx {
class SurfaceTexture;
}
#endif

namespace gpu {

namespace gles2 {
class GLES2Implementation;
}

// The default uninitialized value is -1.
struct GLES2_IMPL_EXPORT GLInProcessContextAttribs {
  GLInProcessContextAttribs();

  int32 alpha_size;
  int32 blue_size;
  int32 green_size;
  int32 red_size;
  int32 depth_size;
  int32 stencil_size;
  int32 samples;
  int32 sample_buffers;
  int32 fail_if_major_perf_caveat;
};

class GLES2_IMPL_EXPORT GLInProcessContext {
 public:
  virtual ~GLInProcessContext() {}

  // Create a GLInProcessContext, if |is_offscreen| is true, renders to an
  // offscreen context. |attrib_list| must be NULL or a NONE-terminated list
  // of attribute/value pairs.
  static GLInProcessContext* CreateContext(
      bool is_offscreen,
      gfx::AcceleratedWidget window,
      const gfx::Size& size,
      bool share_resources,
      const GLInProcessContextAttribs& attribs,
      gfx::GpuPreference gpu_preference);

  // Create context with the provided GLSurface. All other arguments match
  // CreateContext factory above. Can only be called if the command buffer
  // service runs on the same thread as this client because GLSurface is not
  // thread safe.
  static GLInProcessContext* CreateWithSurface(
      scoped_refptr<gfx::GLSurface> surface,
      scoped_refptr<gpu::InProcessCommandBuffer::Service> service,
      GLInProcessContext* share_context,
      const GLInProcessContextAttribs& attribs,
      gfx::GpuPreference gpu_preference);

  virtual void SetContextLostCallback(const base::Closure& callback) = 0;

  // Allows direct access to the GLES2 implementation so a GLInProcessContext
  // can be used without making it current.
  virtual gles2::GLES2Implementation* GetImplementation() = 0;

#if defined(OS_ANDROID)
  virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) = 0;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_
