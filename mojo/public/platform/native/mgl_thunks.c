// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/platform/native/mgl_thunks.h"

#include <assert.h>

#include "mojo/public/platform/native/thunk_export.h"

static struct MGLThunks g_thunks = {0};

MGLContext MGLCreateContext(MGLOpenGLAPIVersion version,
                            MojoHandle command_buffer_handle,
                            MGLContext share_group,
                            MGLContextLostCallback lost_callback,
                            void* lost_callback_closure,
                            const struct MojoAsyncWaiter* async_waiter) {
  assert(g_thunks.MGLCreateContext);
  return g_thunks.MGLCreateContext(
      version, command_buffer_handle, share_group, lost_callback,
      lost_callback_closure, async_waiter);
}

void MGLDestroyContext(MGLContext context) {
  assert(g_thunks.MGLDestroyContext);
  g_thunks.MGLDestroyContext(context);
}

void MGLMakeCurrent(MGLContext context) {
  assert(g_thunks.MGLMakeCurrent);
  g_thunks.MGLMakeCurrent(context);
}

MGLContext MGLGetCurrentContext(void) {
  assert(g_thunks.MGLGetCurrentContext);
  return g_thunks.MGLGetCurrentContext();
}

THUNK_EXPORT size_t MojoSetMGLThunks(
    const struct MGLThunks* mgl_thunks) {
  if (mgl_thunks->size >= sizeof(g_thunks))
    g_thunks = *mgl_thunks;
  return sizeof(g_thunks);
}
