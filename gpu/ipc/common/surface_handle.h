// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SURFACE_HANDLE_H_
#define GPU_IPC_COMMON_SURFACE_HANDLE_H_

#include "ui/gfx/native_widget_types.h"

namespace gpu {

// TODO(piman): remove PluginWindowHandle and move here when NPAPI is gone.
using SurfaceHandle = gfx::PluginWindowHandle;
const SurfaceHandle kNullSurfaceHandle = gfx::kNullPluginWindow;

}  // namespace gpu

#endif  // GPU_IPC_COMMON_SURFACE_HANDLE_H_
