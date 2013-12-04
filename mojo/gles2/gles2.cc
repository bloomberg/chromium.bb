// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/gles2/gles2.h"

#include "gpu/command_buffer/client/gles2_lib.h"

extern "C" {

void MojoGLES2Initialize() {
  gles2::Initialize();
}

void MojoGLES2Terminate() {
  gles2::Terminate();
}

void MojoGLES2MakeCurrent(uint64_t encoded) {
  // Ack, Hans! It's the giant hack.
  // TODO(abarth): Replace this hack with something more disciplined. Most
  // likley, we should receive a MojoHandle that we use to wire up the
  // client side of an out-of-process command buffer. Given that we're
  // still in-process, we just reinterpret_cast the value into a GL interface.
  gpu::gles2::GLES2Interface* gl_interface =
      reinterpret_cast<gpu::gles2::GLES2Interface*>(
          static_cast<uintptr_t>(encoded));
  gles2::SetGLContext(gl_interface);
}

void MojoGLES2SwapBuffers() {
  gles2::GetGLContext()->SwapBuffers();
}

}  // extern "C"
