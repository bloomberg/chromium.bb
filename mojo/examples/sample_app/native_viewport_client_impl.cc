// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/sample_app/native_viewport_client_impl.h"

#include <stdio.h>

#include "gpu/command_buffer/client/gles2_interface.h"

namespace mojo {
namespace examples {

NativeViewportClientImpl::NativeViewportClientImpl(ScopedMessagePipeHandle pipe)
    : service_(pipe.Pass()) {
  service_.SetPeer(this);
}

NativeViewportClientImpl::~NativeViewportClientImpl() {
}

void NativeViewportClientImpl::DidOpen() {
  printf("NativeViewportClientImpl::DidOpen\n");
}

void NativeViewportClientImpl::DidCreateGLContext(uint64_t encoded_gl) {
  // Ack, Hans! It's the giant hack.
  // TODO(abarth): Replace this hack with something more disciplined. Most
  // likley, we should receive a MojoHandle that we pass off to a lib that
  // populates the normal C API for GL.
  gpu::gles2::GLES2Interface* gl =
      reinterpret_cast<gpu::gles2::GLES2Interface*>(
          static_cast<uintptr_t>(encoded_gl));

  gl->ClearColor(0, 1, 0, 0);
  gl->Clear(GL_COLOR_BUFFER_BIT);
  gl->SwapBuffers();
}

}  // examples
}  // mojo
