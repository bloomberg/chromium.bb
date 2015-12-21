// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/platform_handle/platform_handle_private_thunks.h"

#include <assert.h>
#include <stddef.h>

#if defined(WIN32)
#define THUNK_EXPORT __declspec(dllexport)
#else
#define THUNK_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

static MojoPlatformHandlePrivateThunks g_thunks = {0};

MojoResult MojoCreatePlatformHandleWrapper(MojoPlatformHandle platform_handle,
                                           MojoHandle* wrapper) {
  assert(g_thunks.CreatePlatformHandleWrapper);
  return g_thunks.CreatePlatformHandleWrapper(platform_handle, wrapper);
}

MojoResult MojoExtractPlatformHandle(MojoHandle wrapper,
                                     MojoPlatformHandle* platform_handle) {
  assert(g_thunks.ExtractPlatformHandle);
  return g_thunks.ExtractPlatformHandle(wrapper, platform_handle);
}

extern "C" THUNK_EXPORT size_t MojoSetPlatformHandlePrivateThunks(
    const MojoPlatformHandlePrivateThunks* thunks) {
  if (thunks->size >= sizeof(g_thunks))
    g_thunks = *thunks;
  return sizeof(g_thunks);
}

}  // extern "C"
