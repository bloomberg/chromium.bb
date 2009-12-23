// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_interface.h"

namespace gles2 {

GLInterface* GLInterface::interface_;

void GLInterface::SetGLInterface(GLInterface* gl_interface) {
  interface_ = gl_interface;
}

GLInterface* GLInterface::GetGLInterface() {
  return interface_;
}

}  // namespace gles2

