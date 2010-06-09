// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_GL_GL_IMPLEMENTATION_H_
#define APP_GFX_GL_GL_IMPLEMENTATION_H_

namespace gfx {

// The GL implementation currently in use.
enum GLImplementation {
  kGLImplementationNone,
  kGLImplementationDesktopGL,
  kGLImplementationOSMesaGL,
  kGLImplementationEGLGLES2,
  kGLImplementationMockGL
};

// Initialize a particular GL implementation.
bool InitializeGLBindings(GLImplementation implementation);

// Get the current GL implementation.
GLImplementation GetGLImplementation();

// Find an entry point in the current GL implementation.
void* GetGLProcAddress(const char* name);

}  // namespace gfx

#endif  // APP_GFX_GL_GL_IMPLEMENTATION_H_
