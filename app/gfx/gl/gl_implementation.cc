// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_implementation.h"

#include <algorithm>
#include <string>

#include "app/app_switches.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"

namespace gfx {

const char kGLImplementationDesktopName[] = "desktop";
const char kGLImplementationOSMesaName[]  = "osmesa";
const char kGLImplementationEGLName[]     = "egl";
const char kGLImplementationMockName[]    = "mock";

namespace {

typedef std::vector<base::NativeLibrary> LibraryArray;

GLImplementation g_gl_implementation = kGLImplementationNone;
LibraryArray* g_libraries;
GLGetProcAddressProc g_get_proc_address;

void CleanupNativeLibraries(void* unused) {
  if (g_libraries) {
    for (LibraryArray::iterator it = g_libraries->begin();
         it != g_libraries->end(); ++it) {
      base::UnloadNativeLibrary(*it);
    }
    delete g_libraries;
    g_libraries = NULL;
  }
}
}

GLImplementation GetNamedGLImplementation(const std::string& name) {
  static const struct {
    const char* name;
    GLImplementation implemention;
  } name_pairs[] = {
    { kGLImplementationDesktopName, kGLImplementationDesktopGL },
    { kGLImplementationOSMesaName, kGLImplementationOSMesaGL },
    { kGLImplementationEGLName, kGLImplementationEGLGLES2 },
    { kGLImplementationMockName, kGLImplementationMockGL }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(name_pairs); ++i) {
    if (name == name_pairs[i].name)
      return name_pairs[i].implemention;
  }

  return kGLImplementationNone;
}

bool InitializeBestGLBindings(
    const GLImplementation* allowed_implementations_begin,
    const GLImplementation* allowed_implementations_end) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL)) {
    std::string requested_implementation_name =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kUseGL);
    GLImplementation requested_implementation =
        GetNamedGLImplementation(requested_implementation_name);
    if (std::find(allowed_implementations_begin,
                  allowed_implementations_end,
                  requested_implementation) == allowed_implementations_end) {
      LOG(ERROR) << "Requested GL implementation is not allowed.";
      return false;
    }

    if (InitializeGLBindings(requested_implementation))
      return true;
  } else {
    for (const GLImplementation* p = allowed_implementations_begin;
         p < allowed_implementations_end;
         ++p) {
      if (InitializeGLBindings(*p))
        return true;
    }
  }

  LOG(ERROR) << "Could not initialize GL.";
  return false;
}

void SetGLImplementation(GLImplementation implementation) {
  g_gl_implementation = implementation;
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation;
}

void AddGLNativeLibrary(base::NativeLibrary library) {
  DCHECK(library);

  if (!g_libraries) {
    g_libraries = new LibraryArray;
    base::AtExitManager::RegisterCallback(CleanupNativeLibraries, NULL);
  }

  g_libraries->push_back(library);
}

void SetGLGetProcAddressProc(GLGetProcAddressProc proc) {
  DCHECK(proc);
  g_get_proc_address = proc;
}

void* GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation != kGLImplementationNone);

  if (g_libraries) {
    for (size_t i = 0; i < g_libraries->size(); ++i) {
      void* proc = base::GetFunctionPointerFromNativeLibrary((*g_libraries)[i],
                                                             name);
      if (proc)
        return proc;
    }
  }

  if (g_get_proc_address) {
    void* proc = g_get_proc_address(name);
    if (proc)
      return proc;
  }

  return NULL;
}

}  // namespace gfx
