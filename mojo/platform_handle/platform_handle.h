// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_H_
#define MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
typedef HANDLE MojoPlatformHandle;
#elif defined(OS_POSIX)
typedef int MojoPlatformHandle;
#endif

#endif  // MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_FUNCTIONS_H_
