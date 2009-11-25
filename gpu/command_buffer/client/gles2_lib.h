// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These functions emluate GLES2 over command buffers.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_LIB_H
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_LIB_H

#include "gpu/command_buffer/client/gles2_implementation.h"

namespace gles2 {

extern ::command_buffer::gles2::GLES2Implementation* g_gl_impl;

inline ::command_buffer::gles2::GLES2Implementation* GetGLContext() {
  return g_gl_impl;
}

// Initializes the GLES2 library.
bool InitGLES2Lib();

}  // namespace gles2

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_LIB_H

