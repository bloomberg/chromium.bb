// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_PLATFORM_CHANNEL_HANDLE_H_
#define MOJO_SYSTEM_PLATFORM_CHANNEL_HANDLE_H_

#include "build/build_config.h"

namespace mojo {
namespace system {

#if defined(OS_POSIX)
struct PlatformChannelHandle {
  explicit PlatformChannelHandle(int fd) : fd(fd) {}

  int fd;
};
#else
#error "Platform not yet supported."
#endif

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_PLATFORM_CHANNEL_HANDLE_H_
