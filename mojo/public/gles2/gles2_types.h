// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GLES2_GLES2_TYPES_H_
#define MOJO_PUBLIC_GLES2_GLES2_TYPES_H_

// Note: This header should be compilable as C.

#include <stdint.h>

#include "mojo/public/gles2/gles2_export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MojoGLES2ContextPrivate *MojoGLES2Context;
// TODO(piman):
// - create context synchronously
// - pass width/height through native viewport, not here.
typedef void (*MojoGLES2ContextCreated)(
    void* closure, uint32_t width, uint32_t height);
typedef void (*MojoGLES2ContextLost)(void* closure);
typedef void (*MojoGLES2DrawAnimationFrame)(void* closure);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_GLES2_GLES2_TYPES_H_
