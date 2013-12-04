// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GLES2_ADAPTOR_H_
#define MOJO_PUBLIC_GLES2_ADAPTOR_H_

// Note: This header should be compilable as C.

#include <stdint.h>

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_GLES2_IMPLEMENTATION)
#define MOJO_GLES2_EXPORT __declspec(dllexport)
#else
#define MOJO_GLES2_EXPORT __declspec(dllimport)
#endif  // defined(GFX_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_GLES2_IMPLEMENTATION)
#define MOJO_GLES2_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_GLES2_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_GLES2_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

MOJO_GLES2_EXPORT void MojoGLES2Initialize();
MOJO_GLES2_EXPORT void MojoGLES2Terminate();
// TODO(abarth): MojoGLES2MakeCurrent should take a MojoHandle.
MOJO_GLES2_EXPORT void MojoGLES2MakeCurrent(uint64_t encoded);
MOJO_GLES2_EXPORT void MojoGLES2SwapBuffers();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_GLES2_ADAPTOR_H_
