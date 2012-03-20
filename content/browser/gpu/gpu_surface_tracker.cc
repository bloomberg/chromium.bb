// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_surface_tracker.h"

#include "base/logging.h"

GpuSurfaceTracker::GpuSurfaceTracker()
    : next_surface_id_(1) {
}

GpuSurfaceTracker::~GpuSurfaceTracker() {
}

GpuSurfaceTracker* GpuSurfaceTracker::GetInstance() {
  return Singleton<GpuSurfaceTracker>::get();
}

int GpuSurfaceTracker::AddSurfaceForRenderer(int renderer_id,
                                             int render_widget_id) {
  base::AutoLock lock(lock_);
  SurfaceInfo info = {
    renderer_id,
    render_widget_id,
    gfx::kNullAcceleratedWidget
  };
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] = info;
  return surface_id;
}

int GpuSurfaceTracker::LookupSurfaceForRenderer(int renderer_id,
                                                int render_widget_id) {
  base::AutoLock lock(lock_);
  for (SurfaceMap::iterator it = surface_map_.begin(); it != surface_map_.end();
       ++it) {
    const SurfaceInfo& info = it->second;
    if (info.renderer_id == renderer_id &&
        info.render_widget_id == render_widget_id) {
      return it->first;
    }
  }
  return 0;
}

int GpuSurfaceTracker::AddSurfaceForNativeWidget(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  SurfaceInfo info = { 0, 0, widget };
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] = info;
  return surface_id;
}

void GpuSurfaceTracker::RemoveSurface(int surface_id) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  surface_map_.erase(surface_id);
}

bool GpuSurfaceTracker::GetRenderWidgetIDForSurface(int surface_id,
                                                    int* renderer_id,
                                                    int* render_widget_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return false;
  const SurfaceInfo& info = it->second;
  *renderer_id = info.renderer_id;
  *render_widget_id = info.render_widget_id;
  return true;
}

void GpuSurfaceTracker::SetSurfaceHandle(int surface_id,
                                         const gfx::GLSurfaceHandle& handle) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  SurfaceInfo& info = surface_map_[surface_id];
  info.handle = handle;
}

gfx::GLSurfaceHandle GpuSurfaceTracker::GetSurfaceHandle(int surface_id) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  return surface_map_[surface_id].handle;
}

gfx::PluginWindowHandle GpuSurfaceTracker::GetSurfaceWindowHandle(
    int surface_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return gfx::kNullPluginWindow;
  return it->second.handle.handle;
}
