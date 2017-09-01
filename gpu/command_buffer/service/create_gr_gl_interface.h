// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CREATE_GR_GL_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_CREATE_GR_GL_INTERFACE_H_

#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace gl {
struct GLVersionInfo;
}

namespace gpu {
namespace gles2 {

// Creates a GrGLInterface by taking function pointers from the current
// GL bindings.
sk_sp<const GrGLInterface> CreateGrGLInterface(
    const gl::GLVersionInfo& version_info);

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_CREATE_GR_GL_INTERFACE_H_
