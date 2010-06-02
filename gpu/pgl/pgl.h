// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_PGL_PGL_H
#define GPU_PGL_PGL_H

#include <npapi.h>
#include <npapi_extensions.h>

#define PGL_TRUE          1
#define PGL_FALSE         0

#define PGL_NO_CONTEXT    ((PGLContext) 0)

#ifdef __cplusplus
extern "C" {
#endif

typedef void* PGLContext;
typedef unsigned int PGLBoolean;
typedef int32_t PGLInt;

// These are the same error codes as used by EGL.
enum {
  PGL_SUCCESS             = 0x3000,
  PGL_NOT_INITIALIZED     = 0x3001,
  PGL_BAD_CONTEXT         = 0x3006,
  PGL_BAD_PARAMETER       = 0x300C,
  PGL_CONTEXT_LOST        = 0x300E
};

// Initialize the PGL library. This must have completed before any other PGL
// functions are invoked.
PGLBoolean pglInitialize();

// Terminate the PGL library. This must be called after any other PGL functions
// have completed.
PGLBoolean pglTerminate();

// Create A PGL context from a Pepper 3D device context.
PGLContext pglCreateContext(NPP npp,
                            NPDevice* device,
                            NPDeviceContext3D* device_context);

// Set the current PGL context for the calling thread.
PGLBoolean pglMakeCurrent(PGLContext pgl_context);

// Get the calling thread's current PGL context.
PGLContext pglGetCurrentContext(void);

// Gets the address of a function.
void (*pglGetProcAddress(char const * procname))();

// Display everything that has been rendered since the last call.
PGLBoolean pglSwapBuffers(void);

// Destroy the given PGL context.
PGLBoolean pglDestroyContext(PGLContext pgl_context);

// Return the current PGL error.
PGLInt pglGetError();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // GPU_PGL_PGL_H
