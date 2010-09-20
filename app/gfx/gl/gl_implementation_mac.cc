// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"

namespace gfx {
namespace {
const char kOpenGLFrameworkPath[] =
    "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL";
}  // namespace anonymous

bool InitializeGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  if (GetGLImplementation() != kGLImplementationNone)
    return true;

  switch (implementation) {
    case kGLImplementationOSMesaGL: {
      FilePath module_path;
      if (!PathService::Get(base::DIR_MODULE, &module_path)) {
        LOG(ERROR) << "PathService::Get failed.";
        return false;
      }

      // When using OSMesa, just use OSMesaGetProcAddress to find entry points.
      base::NativeLibrary library = base::LoadNativeLibrary(
          module_path.Append("libosmesa.dylib"));
      if (!library) {
        DLOG(INFO) << "libosmesa.so not found";
        return false;
      }

      GLGetProcAddressProc get_proc_address =
          reinterpret_cast<GLGetProcAddressProc>(
              base::GetFunctionPointerFromNativeLibrary(
                  library, "OSMesaGetProcAddress"));

      SetGLGetProcAddressProc(get_proc_address);
      AddGLNativeLibrary(library);
      SetGLImplementation(kGLImplementationOSMesaGL);

      InitializeGLBindingsGL();
      InitializeGLBindingsOSMESA();
      break;
    }
    case kGLImplementationDesktopGL: {
      base::NativeLibrary library = base::LoadNativeLibrary(
          FilePath(kOpenGLFrameworkPath));
      if (!library) {
        LOG(ERROR) << "OpenGL framework not found";
        return false;
      }

      AddGLNativeLibrary(library);
      SetGLImplementation(kGLImplementationDesktopGL);

      InitializeGLBindingsGL();
      break;
    }
    case kGLImplementationMockGL: {
      SetGLGetProcAddressProc(GetMockGLProcAddress);
      SetGLImplementation(kGLImplementationMockGL);
      InitializeGLBindingsGL();
      break;
    }
    default:
      return false;
  }

  return true;
}

}  // namespace gfx
