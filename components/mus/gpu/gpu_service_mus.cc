// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/gpu_service_mus.h"

#include "gpu/ipc/common/surface_handle.h"

namespace mus {

GpuServiceMus::GpuServiceMus() {}

GpuServiceMus::~GpuServiceMus() {}

void GpuServiceMus::DidCreateOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::DidDestroyChannel(int client_id) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::DidDestroyOffscreenContext(const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::DidLoseContext(bool offscreen,
                                   gpu::error::ContextLostReason reason,
                                   const GURL& active_url) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::GpuMemoryUmaStats(const gpu::GPUMemoryUmaStats& params) {
  NOTIMPLEMENTED();
}

void GpuServiceMus::StoreShaderToDisk(int32_t client_id,
                                      const std::string& key,
                                      const std::string& shader) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void GpuServiceMus::SendAcceleratedSurfaceCreatedChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  NOTIMPLEMENTED();
}
#endif

void GpuServiceMus::SetActiveURL(const GURL& url) {
  NOTIMPLEMENTED();
}

}  // namespace mus
