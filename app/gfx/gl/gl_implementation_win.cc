// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_implementation.h"

namespace gfx {

namespace {

typedef void* (GL_BINDING_CALL *GetProcAddressProc)(const char* name);

GLImplementation g_gl_implementation = kGLImplementationNone;
std::vector<HMODULE> g_modules;
static GetProcAddressProc g_get_proc_address;

void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (g_gl_implementation != kGLImplementationNone)
    return true;

  HMODULE module;
  switch (implementation) {
    case kGLImplementationOSMesaGL:
      // When using OSMesa, just use OSMesaGetProcAddress to find entry points.
      module = LoadLibraryA("osmesa.dll");
      if (!module)
        return false;

      g_gl_implementation = kGLImplementationOSMesaGL;

      g_get_proc_address = reinterpret_cast<GetProcAddressProc>(
          GetProcAddress(module, "OSMesaGetProcAddress"));
      DCHECK(g_get_proc_address);

      InitializeGLBindingsGL();
      InitializeGLBindingsOSMESA();
      break;

    case kGLImplementationEGLGLES2:
      // When using EGL, first try eglGetProcAddress and then Windows
      // GetProcAddress on both the EGL and GLES2 DLLs.
      module = LoadLibraryA("libegl.dll");
      if (!module)
        return false;

      g_gl_implementation = kGLImplementationEGLGLES2;

      g_get_proc_address = reinterpret_cast<GetProcAddressProc>(
          GetProcAddress(module, "eglGetProcAddress"));
      DCHECK(g_get_proc_address);

      g_modules.push_back(module);

      module = LoadLibraryA("libglesv2.dll");
      DCHECK(module);

      g_modules.push_back(module);

      InitializeGLBindingsGL();
      InitializeGLBindingsEGL();

      // These two functions take single precision float ranther than double
      // precision float parameters in GLES.
      ::gfx::g_glClearDepth = MarshalClearDepthToClearDepthf;
      ::gfx::g_glDepthRange = MarshalDepthRangeToDepthRangef;
      break;

    case kGLImplementationDesktopGL:
      // When using Windows OpenGL, first try wglGetProcAddress and then
      // Windows GetProcAddress.
      module = LoadLibraryA("opengl32.dll");
      if (!module)
        return false;

      g_gl_implementation = kGLImplementationDesktopGL;

      g_get_proc_address = reinterpret_cast<GetProcAddressProc>(
          GetProcAddress(module, "wglGetProcAddress"));
      DCHECK(g_get_proc_address);

      g_modules.push_back(module);

      InitializeGLBindingsGL();
      InitializeGLBindingsWGL();
      break;

    case kGLImplementationMockGL:
      g_get_proc_address = GetMockGLProcAddress;
      g_gl_implementation = kGLImplementationMockGL;
      InitializeGLBindingsGL();
      break;

    default:
      return false;
  }

  return true;
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation;
}

void* GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation != kGLImplementationNone);

  if (g_get_proc_address) {
    void* proc = g_get_proc_address(name);
    if (proc)
      return proc;
  }

  for (size_t i = 0; i < g_modules.size(); ++i) {
    void* proc = GetProcAddress(g_modules[i], name);
    if (proc)
      return proc;
  }

  return NULL;
}

}  // namespace gfx
