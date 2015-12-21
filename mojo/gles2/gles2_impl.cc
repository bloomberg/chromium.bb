// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/gles2/gles2_context.h"
// Even though this isn't used here, we need to include it to get the symbols to
// be exported in component build.
#include "mojo/public/c/gles2/chromium_extension.h"
#include "mojo/public/c/gles2/gles2.h"

using gles2::GLES2Context;

namespace {

const int32_t kNone = 0x3038;  // EGL_NONE

base::LazyInstance<base::ThreadLocalPointer<gpu::gles2::GLES2Interface> >::Leaky
    g_gpu_interface;

void RunSignalSyncCallback(MojoGLES2SignalSyncPointCallback callback,
                           void* closure) {
  callback(closure);
}

}  // namespace

extern "C" {
MojoGLES2Context MojoGLES2CreateContext(MojoHandle handle,
                                        const int32_t* attrib_list,
                                        MojoGLES2ContextLost lost_callback,
                                        void* closure,
                                        const MojoAsyncWaiter* async_waiter) {
  mojo::MessagePipeHandle mph(handle);
  mojo::ScopedMessagePipeHandle scoped_handle(mph);
  std::vector<int32_t> attribs;
  if (attrib_list) {
    for (int32_t const* p = attrib_list; *p != kNone;) {
      attribs.push_back(*p++);
      attribs.push_back(*p++);
    }
  }
  attribs.push_back(kNone);
  scoped_ptr<GLES2Context> client(new GLES2Context(
      attribs, async_waiter, std::move(scoped_handle), lost_callback, closure));
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
  g_gpu_interface.Get().Set(interface);
}

void MojoGLES2SwapBuffers() {
  DCHECK(g_gpu_interface.Get().Get());
  g_gpu_interface.Get().Get()->SwapBuffers();
}

void MojoGLES2SignalSyncPoint(
    MojoGLES2Context context,
    uint32_t sync_point,
    MojoGLES2SignalSyncPointCallback callback,
    void* closure) {
  DCHECK(context);
  static_cast<GLES2Context*>(context)->context_support()->SignalSyncPoint(
      sync_point, base::Bind(&RunSignalSyncCallback, callback, closure));
}

void* MojoGLES2GetGLES2Interface(MojoGLES2Context context) {
  return static_cast<GLES2Context*>(context)->interface();
}

void* MojoGLES2GetContextSupport(MojoGLES2Context context) {
  return static_cast<GLES2Context*>(context)->context_support();
}

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType GL_APIENTRY gl##Function PARAMETERS {                 \
    DCHECK(g_gpu_interface.Get().Get());                           \
    return g_gpu_interface.Get().Get()->Function ARGUMENTS;        \
  }
#include "mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#include "mojo/public/c/gles2/gles2_call_visitor_chromium_extension_autogen.h"
#undef VISIT_GL_CALL

}  // extern "C"
