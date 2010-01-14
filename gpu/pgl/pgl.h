// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_PGL_PGL_H
#define GPU_PGL_PGL_H

#include "npapi.h"
#include "npapi_extensions.h"

extern "C" {
typedef void* PGLContext;
typedef bool PGLBoolean;

PGLContext pglCreateContext(NPP npp,
                            NPDevice* device,
                            NPDeviceContext3D* device_context);
PGLBoolean pglMakeCurrent(PGLContext pgl_context);
PGLBoolean pglSwapBuffers();
PGLBoolean pglDestroyContext(PGLContext pgl_context);
}  // extern "C"

#endif  // GPU_PGL_PGL_H