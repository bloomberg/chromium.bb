// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GLES2_GLES2_H_
#define MOJO_PUBLIC_GLES2_GLES2_H_

// Note: This header should be compilable as C.

#include <stdint.h>
#include <GLES2/gl2.h>

#include "mojo/public/gles2/gles2_export.h"

#ifdef __cplusplus
extern "C" {
#endif

MOJO_GLES2_EXPORT void MojoGLES2Initialize();
MOJO_GLES2_EXPORT void MojoGLES2Terminate();
// TODO(abarth): MojoGLES2MakeCurrent should take a MojoHandle.
MOJO_GLES2_EXPORT void MojoGLES2MakeCurrent(uint64_t encoded);
MOJO_GLES2_EXPORT void MojoGLES2SwapBuffers();
#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  MOJO_GLES2_EXPORT ReturnType GL_APIENTRY gl##Function PARAMETERS;
#include "mojo/public/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_GLES2_GLES2_H_
