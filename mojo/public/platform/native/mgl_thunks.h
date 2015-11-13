// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_PLATFORM_NATIVE_MGL_THUNKS_H_
#define MOJO_PUBLIC_PLATFORM_NATIVE_MGL_THUNKS_H_

#include <stddef.h>

#include "mojo/public/c/gpu/MGL/mgl.h"

// Structure used to bind the interface which manipulates MGL contexts to a
// DSO to theose of the embedder.
//
// This is the ABI between the embedder and the DSO. It can only have new
// functions added to the end. No other changes are supported.
#pragma pack(push, 8)
struct MGLThunks {
  size_t size;  // Should be set to sizeof(MGLThunks).

  MGLContext (*MGLCreateContext)(MGLOpenGLAPIVersion version,
                                 MojoHandle command_buffer_handle,
                                 MGLContext share_group,
                                 MGLContextLostCallback lost_callback,
                                 void* lost_callback_closure,
                                 const struct MojoAsyncWaiter* async_waiter);
  void (*MGLDestroyContext)(MGLContext context);
  void (*MGLMakeCurrent)(MGLContext context);
  MGLContext (*MGLGetCurrentContext)(void);
};
#pragma pack(pop)

// Intended to be called from the embedder. Returns an object initialized to
// contain pointers to each of the embedder's MGLThunks functions.
inline struct MGLThunks MojoMakeMGLThunks() {
  struct MGLThunks mgl_thunks = {
      sizeof(struct MGLThunks),
      MGLCreateContext,
      MGLDestroyContext,
      MGLMakeCurrent,
      MGLGetCurrentContext,
  };

  return mgl_thunks;
}

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system. For example:
// MojoSetMGLThunksFn mojo_set_gles2_thunks_fn =
//     reinterpret_cast<MojoSetMGLThunksFn>(
//         app_library.GetFunctionPointer("MojoSetMGLThunks"));
// The expected size of |mgl_thunks| is returned.
// The contents of |mgl_thunks| are copied.
typedef size_t (*MojoSetMGLThunksFn)(const struct MGLThunks* mgl_thunks);

#endif  // MOJO_PUBLIC_PLATFORM_NATIVE_MGL_THUNKS_H_
