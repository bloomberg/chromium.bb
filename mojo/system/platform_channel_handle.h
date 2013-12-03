// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PLATFORM_CHANNEL_HANDLE_H_
#define MOJO_SYSTEM_PLATFORM_CHANNEL_HANDLE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace system {

#if defined(OS_POSIX)
struct PlatformChannelHandle {
  PlatformChannelHandle() : fd(-1) {}
  explicit PlatformChannelHandle(int fd) : fd(fd) {}

  void CloseIfNecessary();

  bool is_valid() const { return fd != -1; }

  int fd;
};
#elif defined(OS_WIN)
struct PlatformChannelHandle {
  PlatformChannelHandle() : handle(INVALID_HANDLE_VALUE) {}
  explicit PlatformChannelHandle(HANDLE handle) : handle(handle) {}

  void CloseIfNecessary();

  bool is_valid() const { return handle != INVALID_HANDLE_VALUE; }

  HANDLE handle;
};
#else
#error "Platform not yet supported."
#endif

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PLATFORM_CHANNEL_HANDLE_H_
