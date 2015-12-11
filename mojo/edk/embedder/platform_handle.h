// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_H_
#define MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_H_

#include "build/build_config.h"
#include "mojo/edk/system/system_impl_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace edk {

#if defined(OS_POSIX)
struct MOJO_SYSTEM_IMPL_EXPORT PlatformHandle {
  PlatformHandle() : handle(-1) {}
  explicit PlatformHandle(int handle) : handle(handle) {}

  void CloseIfNecessary();

  bool is_valid() const { return handle != -1; }

  int handle;
};
#elif defined(OS_WIN)
struct MOJO_SYSTEM_IMPL_EXPORT PlatformHandle {
  PlatformHandle() : handle(INVALID_HANDLE_VALUE) {}
  explicit PlatformHandle(HANDLE handle) : handle(handle) {}

  void CloseIfNecessary();

  bool is_valid() const { return handle != INVALID_HANDLE_VALUE; }

  HANDLE handle;
};
#else
#error "Platform not yet supported."
#endif

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_H_
