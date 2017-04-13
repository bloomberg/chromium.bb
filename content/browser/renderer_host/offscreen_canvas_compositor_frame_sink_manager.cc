// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_compositor_frame_sink_manager.h"

#include "base/lazy_instance.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"

namespace content {

namespace {
base::LazyInstance<OffscreenCanvasCompositorFrameSinkManager>::Leaky g_manager =
    LAZY_INSTANCE_INITIALIZER;
}

OffscreenCanvasCompositorFrameSinkManager::
    OffscreenCanvasCompositorFrameSinkManager() {
  GetSurfaceManager()->AddObserver(this);
}

OffscreenCanvasCompositorFrameSinkManager::
    ~OffscreenCanvasCompositorFrameSinkManager() {
  registered_surface_instances_.clear();
  GetSurfaceManager()->RemoveObserver(this);
}

OffscreenCanvasCompositorFrameSinkManager*
OffscreenCanvasCompositorFrameSinkManager::GetInstance() {
  return g_manager.Pointer();
}

void OffscreenCanvasCompositorFrameSinkManager::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  auto surface_iter =
      registered_surface_instances_.find(surface_info.id().frame_sink_id());
  if (surface_iter == registered_surface_instances_.end())
    return;
  OffscreenCanvasSurfaceImpl* surface_impl = surface_iter->second;
  surface_impl->OnSurfaceCreated(surface_info);
}

void OffscreenCanvasCompositorFrameSinkManager::
    RegisterOffscreenCanvasSurfaceInstance(
        const cc::FrameSinkId& frame_sink_id,
        OffscreenCanvasSurfaceImpl* surface_instance) {
  DCHECK(surface_instance);
  DCHECK_EQ(registered_surface_instances_.count(frame_sink_id), 0u);
  registered_surface_instances_[frame_sink_id] = surface_instance;
}

void OffscreenCanvasCompositorFrameSinkManager::
    UnregisterOffscreenCanvasSurfaceInstance(
        const cc::FrameSinkId& frame_sink_id) {
  DCHECK_EQ(registered_surface_instances_.count(frame_sink_id), 1u);
  registered_surface_instances_.erase(frame_sink_id);
}

OffscreenCanvasSurfaceImpl*
OffscreenCanvasCompositorFrameSinkManager::GetSurfaceInstance(
    const cc::FrameSinkId& frame_sink_id) {
  auto search = registered_surface_instances_.find(frame_sink_id);
  if (search != registered_surface_instances_.end()) {
    return search->second;
  }
  return nullptr;
}

}  // namespace content
