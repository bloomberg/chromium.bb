// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific types and definitions for native widget handles.

#ifndef GPU_DEMOS_FRAMEWORK_PLATFORM_H_
#define GPU_DEMOS_FRAMEWORK_PLATFORM_H_

#ifdef _WINDOWS
#include <windows.h>
#endif  // _WINDOWS

#include "build/build_config.h"

namespace gpu {
namespace demos {

#if defined(OS_WIN)
typedef HWND NativeWindowHandle;
#endif  // defined(OS_WIN)

}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_FRAMEWORK_PLATFORM_H_
