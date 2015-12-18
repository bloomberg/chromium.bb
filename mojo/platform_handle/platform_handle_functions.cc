// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/platform_handle/platform_handle_functions.h"

#include <utility>

#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

extern "C" {

MojoResult MojoCreatePlatformHandleWrapper(MojoPlatformHandle platform_handle,
                                           MojoHandle* wrapper) {
  mojo::embedder::PlatformHandle platform_handle_wrapper(platform_handle);
  mojo::embedder::ScopedPlatformHandle scoped_platform_handle(
      platform_handle_wrapper);
  return mojo::embedder::CreatePlatformHandleWrapper(
      std::move(scoped_platform_handle), wrapper);
}

MojoResult MojoExtractPlatformHandle(MojoHandle wrapper,
                                     MojoPlatformHandle* platform_handle) {
  mojo::embedder::ScopedPlatformHandle scoped_platform_handle;
  MojoResult result = mojo::embedder::PassWrappedPlatformHandle(
      wrapper, &scoped_platform_handle);
  if (result != MOJO_RESULT_OK)
    return result;

  DCHECK(scoped_platform_handle.is_valid());
#if defined(OS_POSIX)
  *platform_handle = scoped_platform_handle.release().fd;
#elif defined(OS_WIN)
  *platform_handle = scoped_platform_handle.release().handle;
#else
#error "Platform not yet supported."
#endif
  return MOJO_RESULT_OK;
}

}  // extern "C"
