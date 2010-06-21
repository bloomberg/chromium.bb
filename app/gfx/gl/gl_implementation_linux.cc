// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <vector>

#include "base/at_exit.h"
#include "base/logging.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context_stub.h"
#include "app/gfx/gl/gl_implementation.h"

namespace gfx {
namespace {
typedef void* (*GetProcAddressProc)(const char* name);

GLImplementation g_gl_implementation = kGLImplementationNone;
typedef std::vector<void*> PointerArray;
PointerArray* g_shared_libraries = NULL;
GetProcAddressProc g_get_proc_address;

// TODO(piman): it should be Desktop GL marshalling from double to float. Today
// on native GLES, we do float->double->float.
void GL_BINDING_CALL MarshalClearDepthToClearDepthf(GLclampd depth) {
  glClearDepthf(static_cast<GLclampf>(depth));
}

void GL_BINDING_CALL MarshalDepthRangeToDepthRangef(GLclampd z_near,
                                                    GLclampd z_far) {
  glDepthRangef(static_cast<GLclampf>(z_near), static_cast<GLclampf>(z_far));
}

void CleanupSharedLibraries(void* unused) {
  if (g_shared_libraries) {
    for (PointerArray::iterator it = g_shared_libraries->begin();
         it != g_shared_libraries->end(); ++it) {
      dlclose(*it);
    }
    delete g_shared_libraries;
    g_shared_libraries = NULL;
  }
}

}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (g_gl_implementation != kGLImplementationNone)
    return true;

  if (!g_shared_libraries) {
    g_shared_libraries = new PointerArray();
    base::AtExitManager::RegisterCallback(CleanupSharedLibraries, NULL);
  }

  void* shared_library = NULL;

  switch (implementation) {
    case kGLImplementationDesktopGL:
      DLOG(INFO) << "Initializing Desktop GL";
      shared_library = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
      if (!shared_library) {
        DLOG(ERROR) << "Failed to load libGL.so.1";
        return false;
      }

      g_shared_libraries->push_back(shared_library);

      g_gl_implementation = kGLImplementationDesktopGL;

      g_get_proc_address = reinterpret_cast<GetProcAddressProc>(
          dlsym(shared_library, "glXGetProcAddress"));
      CHECK(g_get_proc_address);

      InitializeGLBindingsGL();
      InitializeGLBindingsGLX();
      break;

    case kGLImplementationEGLGLES2:
      DLOG(INFO) << "Initializing EGL";
      shared_library = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
      if (!shared_library) {
        DLOG(ERROR) << "Failed to load libEGL.so";
        return false;
      }

      g_gl_implementation = kGLImplementationEGLGLES2;

      g_get_proc_address = reinterpret_cast<GetProcAddressProc>(
          dlsym(shared_library, "eglGetProcAddress"));
      DCHECK(g_get_proc_address);

      g_shared_libraries->push_back(shared_library);

      shared_library = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_LOCAL);
      if (!shared_library) {
        DLOG(ERROR) << "Failed to load libGLESv2.so";
        g_shared_libraries->clear();
        return false;
      }

      DCHECK(shared_library);

      g_shared_libraries->push_back(shared_library);

      InitializeGLBindingsGL();
      InitializeGLBindingsEGL();

      // These two functions take single precision float rather than double
      // precision float parameters in GLES.
      ::gfx::g_glClearDepth = MarshalClearDepthToClearDepthf;
      ::gfx::g_glDepthRange = MarshalDepthRangeToDepthRangef;
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

  if (g_shared_libraries) {
    for (PointerArray::iterator it = g_shared_libraries->begin();
         it != g_shared_libraries->end(); ++it) {
      void* proc = dlsym(*it, name);
      if (proc)
        return proc;
    }
  }

  return NULL;
}

}  // namespace gfx
