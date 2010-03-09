// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_NATIVE_WINDOW_HANDLE_H_
#define CHROME_COMMON_GPU_NATIVE_WINDOW_HANDLE_H_

#include "build/build_config.h"

// This file defines the window handle type used by the GPU process IPC layer.
// This is different than gfx::NativeWindow[Id] since on X, this is an XID.
// Normally, Chrome deals with either GTK window pointers, or magic window
// IDs that the app generates. The GPU process needs to know the real XID.

#if defined(OS_WIN)

#include <windows.h>

typedef HWND GpuNativeWindowHandle;

#elif defined(OS_MACOSX)

// The GPU process isn't supported on Mac yet. Defining this arbitrarily allows
// us to not worry about the integration points not compiling.
typedef int GpuNativeWindowHandle;

#elif defined(USE_X11)

// Forward declar XID ourselves to avoid pulling in all of the X headers, which
// can cause compile problems for some parts of the project.
typedef unsigned long XID;

typedef XID GpuNativeWindowHandle;

#else

#error define GpuNativeWindowHandle

#endif

#endif  // CHROME_COMMON_GPU_NATIVE_WINDOW_HANDLE_H_
