// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These functions emulate GLES2 over command buffers.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_LIB_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_LIB_H_

#include "gpu/command_buffer/client/gles2_c_lib_export.h"

#include "../client/gles2_implementation.h"

namespace gles2 {

// Initialize the GLES2 library.
GLES2_C_LIB_EXPORT void Initialize();

// Terminate the GLES2 library.
GLES2_C_LIB_EXPORT void Terminate();

// Get the current GL context.
GLES2_C_LIB_EXPORT gpu::gles2::GLES2Implementation* GetGLContext();

// Set the current GL context.
GLES2_C_LIB_EXPORT void SetGLContext(gpu::gles2::GLES2Implementation* impl);

}  // namespace gles2

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_LIB_H_
