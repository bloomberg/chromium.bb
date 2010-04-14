// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_context.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/common/logging.h"

namespace gpu {

GLContext::GLContext() {
}

GLContext::~GLContext() {
}

// GLEW initialization is extremely expensive because it looks up
// hundreds of function pointers. Realistically we are not going to
// switch between GL implementations on the fly, so for the time being
// we only do the context-dependent GLEW initialization once.
bool InitializeGLEW() {
#if defined(UNIT_TEST)
  return true;
#else
  static bool initialized = false;
  if (initialized)
    return true;

  // Initializes context-dependent parts of GLEW.
  if (glewInit() != GLEW_OK) {
    LOG(ERROR) << "GLEW failed initialization";
    return false;
  }

  // Check to see that we can use the OpenGL vertex attribute APIs
  // TODO(petersont):  Return false if this check fails, but because some
  // Intel hardware does not support OpenGL 2.0, yet does support all of the
  // extensions we require, we only log an error.  A future CL should change
  // this check to ensure that all of the extension strings we require are
  // present.
  if (!GLEW_VERSION_2_0) {
    DLOG(ERROR) << "GL drivers do not have OpenGL 2.0 functionality.";
  }

  // Check for necessary extensions.
  bool extensions_found = true;
  if (!GLEW_ARB_vertex_buffer_object) {
    // NOTE: Linux NVidia drivers claim to support OpenGL 2.0 when using
    // indirect rendering (e.g. remote X), but it is actually lying. The
    // ARB_vertex_buffer_object functions silently no-op (!) when using
    // indirect rendering, leading to crashes. Fortunately, in that case, the
    // driver claims to not support ARB_vertex_buffer_object, so fail in that
    // case.
    DLOG(ERROR) << "GL drivers do not support vertex buffer objects.";
    extensions_found = false;
  }
  if (!GLEW_EXT_framebuffer_object) {
    DLOG(ERROR) << "GL drivers do not support framebuffer objects.";
    extensions_found = false;
  }
  if (!GLEW_VERSION_2_0 && !GLEW_EXT_stencil_two_side) {
    DLOG(ERROR) << "Two sided stencil extension missing.";
    extensions_found = false;
  }
  if (!GLEW_VERSION_1_4 && !GLEW_EXT_blend_func_separate) {
    DLOG(ERROR) <<"Separate blend func extension missing.";
    extensions_found = false;
  }
  if (!GLEW_VERSION_2_0 && !GLEW_EXT_blend_equation_separate) {
    DLOG(ERROR) << "Separate blend function extension missing.";
    extensions_found = false;
  }
  if (!extensions_found)
    return false;

  initialized = true;
  return true;
#endif
}

bool GLContext::InitializeCommon() {
  if (!MakeCurrent())
    return false;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  if (glGetError() != GL_NO_ERROR)
    return false;

  return true;
}
}  // namespace gpu
