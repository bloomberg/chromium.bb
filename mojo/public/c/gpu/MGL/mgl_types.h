// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_GPU_MGL_TYPES_H_
#define MOJO_PUBLIC_C_GPU_MGL_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MGLContextPrivate* MGLContext;
typedef void (*MGLContextLostCallback)(void* closure);
typedef void (*MGLSignalSyncPointCallback)(void* closure);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_GPU_MGL_TYPES_H_
