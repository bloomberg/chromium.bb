// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_vsync_provider.h"

namespace gpu {

/* static */
std::unique_ptr<GpuVSyncProvider> GpuVSyncProvider::Create(
    const VSyncCallback& callback,
    SurfaceHandle surface_handle) {
  return std::unique_ptr<GpuVSyncProvider>();
}

GpuVSyncProvider::~GpuVSyncProvider() = default;

void GpuVSyncProvider::EnableVSync(bool enabled) {
  NOTREACHED();
}

}  // namespace gpu
