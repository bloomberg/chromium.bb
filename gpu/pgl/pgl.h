// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_PGL_PGL_H
#define GPU_PGL_PGL_H

#include "npapi.h"
#include "npapi_extensions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* PGLContext;
typedef bool PGLBoolean;

// Create A PGL context from a Pepper 3D device context.
PGLContext pglCreateContext(NPP npp,
                            NPDevice* device,
                            NPDeviceContext3D* device_context);

// Set the current PGL context for the calling thread.
PGLBoolean pglMakeCurrent(PGLContext pgl_context);

// Get the calling thread's current PGL context.
PGLContext pglGetCurrentContext(void);

// Display everything that has been rendered since the last call.
PGLBoolean pglSwapBuffers(void);

// Destroy the given PGL context.
PGLBoolean pglDestroyContext(PGLContext pgl_context);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // GPU_PGL_PGL_H
