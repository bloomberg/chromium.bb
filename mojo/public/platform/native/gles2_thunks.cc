// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/platform/native/gles2_thunks.h"

#include <assert.h>

#include "mojo/public/platform/native/thunk_export.h"

extern "C" {

static MojoGLES2ControlThunks g_control_thunks = {0};
static MojoGLES2ImplThunks g_impl_thunks = {0};

MojoGLES2Context MojoGLES2CreateContext(MojoHandle handle,
                                        MojoGLES2ContextLost lost_callback,
                                        void* closure,
                                        const MojoAsyncWaiter* async_waiter) {
  assert(g_control_thunks.GLES2CreateContext);
  return g_control_thunks.GLES2CreateContext(
      handle, lost_callback, closure, async_waiter);
}

void MojoGLES2DestroyContext(MojoGLES2Context context) {
  assert(g_control_thunks.GLES2DestroyContext);
  g_control_thunks.GLES2DestroyContext(context);
}

void MojoGLES2MakeCurrent(MojoGLES2Context context) {
  assert(g_control_thunks.GLES2MakeCurrent);
  g_control_thunks.GLES2MakeCurrent(context);
}

void MojoGLES2SwapBuffers() {
  assert(g_control_thunks.GLES2SwapBuffers);
  g_control_thunks.GLES2SwapBuffers();
}

void* MojoGLES2GetGLES2Interface(MojoGLES2Context context) {
  assert(g_control_thunks.GLES2GetGLES2Interface);
  return g_control_thunks.GLES2GetGLES2Interface(context);
}

void* MojoGLES2GetContextSupport(MojoGLES2Context context) {
  assert(g_control_thunks.GLES2GetContextSupport);
  return g_control_thunks.GLES2GetContextSupport(context);
}

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType gl##Function PARAMETERS {                             \
    assert(g_impl_thunks.Function);                                \
    return g_impl_thunks.Function ARGUMENTS;                       \
  }
#include "mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

extern "C" THUNK_EXPORT size_t MojoSetGLES2ControlThunks(
    const MojoGLES2ControlThunks* gles2_control_thunks) {
  if (gles2_control_thunks->size >= sizeof(g_control_thunks))
    g_control_thunks = *gles2_control_thunks;
  return sizeof(g_control_thunks);
}

extern "C" THUNK_EXPORT size_t MojoSetGLES2ImplThunks(
    const MojoGLES2ImplThunks* gles2_impl_thunks) {
  if (gles2_impl_thunks->size >= sizeof(g_impl_thunks))
    g_impl_thunks = *gles2_impl_thunks;
  return sizeof(g_impl_thunks);
}

}  // extern "C"
