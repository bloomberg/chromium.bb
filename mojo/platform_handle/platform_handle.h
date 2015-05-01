// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_H_
#define MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_WIN)
using MojoPlatformHandle = HANDLE;
#elif defined(OS_POSIX)
using MojoPlatformHandle = int;
#endif

#endif  // MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_FUNCTIONS_H_
