// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/sample_gles2_delegate.h"

#include <stdio.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace mojo {
namespace examples {

SampleGLES2Delegate::SampleGLES2Delegate() {
}

SampleGLES2Delegate::~SampleGLES2Delegate() {
}

void SampleGLES2Delegate::DidCreateContext(GLES2* gl) {
  glClearColor(0, 1, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  gl->SwapBuffers();
}

void SampleGLES2Delegate::ContextLost(GLES2* gl) {
}

}  // namespace examples
}  // namespace mojo
