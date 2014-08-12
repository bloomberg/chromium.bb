// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/gles2/gles2.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/gles2/gles2_context.h"

using mojo::gles2::GLES2Context;

namespace {

const MojoAsyncWaiter* g_async_waiter = NULL;
gpu::gles2::GLES2Interface* g_gpu_interface = NULL;

}  // namespace

extern "C" {

void MojoGLES2Initialize(const MojoAsyncWaiter* async_waiter) {
  DCHECK(!g_async_waiter);
  DCHECK(async_waiter);
  g_async_waiter = async_waiter;
}

void MojoGLES2Terminate() {
  DCHECK(g_async_waiter);
  g_async_waiter = NULL;
}

MojoGLES2Context MojoGLES2CreateContext(
    MojoHandle handle,
    MojoGLES2ContextLost lost_callback,
    void* closure) {
  mojo::MessagePipeHandle mph(handle);
  mojo::ScopedMessagePipeHandle scoped_handle(mph);
  scoped_ptr<GLES2Context> client(new GLES2Context(g_async_waiter,
                                                   scoped_handle.Pass(),
                                                   lost_callback,
                                                   closure));
  if (!client->Initialize())
    client.reset();
  return client.release();
}

void MojoGLES2DestroyContext(MojoGLES2Context context) {
  delete static_cast<GLES2Context*>(context);
}

void MojoGLES2MakeCurrent(MojoGLES2Context context) {
  gpu::gles2::GLES2Interface* interface = NULL;
  if (context) {
    GLES2Context* client = static_cast<GLES2Context*>(context);
    interface = client->interface();
    DCHECK(interface);
  }
  g_gpu_interface = interface;
}

void MojoGLES2SwapBuffers() {
  assert(g_gpu_interface);
  g_gpu_interface->SwapBuffers();
}

void* MojoGLES2GetGLES2Interface(MojoGLES2Context context) {
  return static_cast<GLES2Context*>(context)->interface();
}

void* MojoGLES2GetContextSupport(MojoGLES2Context context) {
  return static_cast<GLES2Context*>(context)->context_support();
}

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS)  \
  ReturnType gl##Function PARAMETERS {                              \
    assert(g_gpu_interface);                                        \
    return g_gpu_interface->Function ARGUMENTS;                     \
  }
#include "mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

}  // extern "C"
