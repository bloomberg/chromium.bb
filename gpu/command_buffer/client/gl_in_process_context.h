// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_
#define GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "gles2_impl_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gpu_preference.h"

namespace gfx {
class Size;
}

namespace gpu {

namespace gles2 {
class GLES2Implementation;
}

class GpuMemoryBufferFactory;

class GLES2_IMPL_EXPORT GLInProcessContext {
 public:
  virtual ~GLInProcessContext() {}

  // Must be called before any GLInProcessContext instances are created.
  static void SetGpuMemoryBufferFactory(GpuMemoryBufferFactory* factory);

  // GLInProcessContext configuration attributes. These are the same as used by
  // EGL. Attributes are matched using a closest fit algorithm.
  enum Attribute {
    ALPHA_SIZE     = 0x3021,
    BLUE_SIZE      = 0x3022,
    GREEN_SIZE     = 0x3023,
    RED_SIZE       = 0x3024,
    DEPTH_SIZE     = 0x3025,
    STENCIL_SIZE   = 0x3026,
    SAMPLES        = 0x3031,
    SAMPLE_BUFFERS = 0x3032,
    NONE           = 0x3038  // Attrib list = terminator
  };

  // Create a GLInProcessContext, if |is_offscreen| is true, renders to an
  // offscreen context. |attrib_list| must be NULL or a NONE-terminated list
  // of attribute/value pairs.
  static GLInProcessContext* CreateContext(
      bool is_offscreen,
      gfx::AcceleratedWidget window,
      const gfx::Size& size,
      bool share_resources,
      const char* allowed_extensions,
      const int32* attrib_list,
      gfx::GpuPreference gpu_preference,
      const base::Closure& callback);

  virtual void SignalSyncPoint(unsigned sync_point,
                               const base::Closure& callback) = 0;

  virtual void SignalQuery(unsigned query, const base::Closure& callback) = 0;

  // Allows direct access to the GLES2 implementation so a GLInProcessContext
  // can be used without making it current.
  virtual gles2::GLES2Implementation* GetImplementation() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GL_IN_PROCESS_CONTEXT_H_
