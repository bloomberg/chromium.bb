// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_lib.h"

namespace gles2 {

::gpu::gles2::GLES2Implementation* g_gl_impl;

bool InitGLES2Lib() {
  // TODO(gman): Encapulate initalizing the GLES2 library for client apps.
  return false;
}

}  // namespace gles2




