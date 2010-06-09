// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "base/logging.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_stub.h"
#include "app/gfx/gl/gl_implementation.h"

namespace gfx {
namespace {
typedef void* (*GetProcAddressProc)(const char* name);

GLImplementation g_gl_implementation = kGLImplementationNone;
void* g_shared_library;
GetProcAddressProc g_get_proc_address;
}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (g_gl_implementation != kGLImplementationNone)
    return true;

  switch (implementation) {
    case kGLImplementationDesktopGL:
      g_shared_library = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
      if (!g_shared_library)
        return false;

      g_gl_implementation = kGLImplementationDesktopGL;

      g_get_proc_address = reinterpret_cast<GetProcAddressProc>(
          dlsym(g_shared_library, "glXGetProcAddress"));
      CHECK(g_get_proc_address);

      InitializeGLBindingsGL();
      InitializeGLBindingsGLX();
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

  if (g_shared_library) {
    void* proc = dlsym(g_shared_library, name);
    if (proc)
      return proc;
  }

  return NULL;
}

}  // namespace gfx
