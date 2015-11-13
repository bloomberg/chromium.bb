// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_GPU_MGL_MGL_ONSCREEN_H_
#define MOJO_PUBLIC_C_GPU_MGL_MGL_ONSCREEN_H_

#include <stdint.h>

#include "mojo/public/c/gpu/MGL/mgl_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Resizes the default framebuffer for the currently bound onscreen MGLContext.
void MGLResizeSurface(uint32_t width, uint32_t height);

// Presents the default framebuffer for the currently bound onscreen MGLContext
// to the windowing system or display.
void MGLSwapBuffers();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_GPU_MGL_MGL_ONSCREEN_H_
