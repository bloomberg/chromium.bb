// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_PLATFORM_NATIVE_MGL_ONSCREEN_THUNKS_H_
#define MOJO_PUBLIC_PLATFORM_NATIVE_MGL_ONSCREEN_THUNKS_H_

#include <stddef.h>

#include "mojo/public/c/gpu/MGL/mgl_onscreen.h"

// Structure used to bind the interface which manipulates MGL contexts to a
// DSO to theose of the embedder.
//
// This is the ABI between the embedder and the DSO. It can only have new
// functions added to the end. No other changes are supported.
#pragma pack(push, 8)
struct MGLOnscreenThunks {
  size_t size;  // Should be set to sizeof(MojoMGLOnscreenThunks).

  void (*MGLResizeSurface)(uint32_t width, uint32_t height);
  void (*MGLSwapBuffers)(void);
};
#pragma pack(pop)

// Intended to be called from the embedder. Returns an object initialized to
// contain pointers to each of the embedder's MGLOnscreenThunks functions.
inline struct MGLOnscreenThunks MojoMakeMGLOnscreenThunks() {
  struct MGLOnscreenThunks mgl_onscreen_thunks = {
      sizeof(struct MGLOnscreenThunks), MGLResizeSurface, MGLSwapBuffers,
  };

  return mgl_onscreen_thunks;
}

// Use this type for the function found by dynamically discovering it in
// a DSO linked with mojo_system. For example:
// MojoSetMGLOnscreenThunksFn mojo_set_gles2_thunks_fn =
//     reinterpret_cast<MojoSetMGLOnscreenThunksFn>(
//         app_library.GetFunctionPointer("MojoSetMGLOnscreenThunks"));
// The expected size of |mgl_thunks| is returned.
// The contents of |mgl_thunks| are copied.
typedef size_t (*MojoSetMGLOnscreenThunksFn)(
    const struct MGLOnscreenThunks* mgl_thunks);

#endif  // MOJO_PUBLIC_PLATFORM_NATIVE_MGL_ONSCREEN_THUNKS_H_
