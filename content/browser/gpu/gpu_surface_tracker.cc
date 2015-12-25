// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_surface_tracker.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#endif  // defined(OS_ANDROID)

namespace content {

GpuSurfaceTracker::GpuSurfaceTracker()
    : next_surface_id_(1) {
  GpuSurfaceLookup::InitInstance(this);
}

GpuSurfaceTracker::~GpuSurfaceTracker() {
  GpuSurfaceLookup::InitInstance(NULL);
}

GpuSurfaceTracker* GpuSurfaceTracker::GetInstance() {
  return base::Singleton<GpuSurfaceTracker>::get();
}

int GpuSurfaceTracker::AddSurfaceForNativeWidget(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] = SurfaceInfo(widget, gfx::GLSurfaceHandle());
  return surface_id;
}

void GpuSurfaceTracker::RemoveSurface(int surface_id) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  surface_map_.erase(surface_id);
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
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return gfx::GLSurfaceHandle();
  return it->second.handle;
}

gfx::AcceleratedWidget GpuSurfaceTracker::AcquireNativeWidget(int surface_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return gfx::kNullAcceleratedWidget;

#if defined(OS_ANDROID)
  if (it->second.native_widget != gfx::kNullAcceleratedWidget)
    ANativeWindow_acquire(it->second.native_widget);
#endif  // defined(OS_ANDROID)

  return it->second.native_widget;
}

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(lock_);
  return surface_map_.size();
}

GpuSurfaceTracker::SurfaceInfo::SurfaceInfo()
    : native_widget(gfx::kNullAcceleratedWidget) {}

GpuSurfaceTracker::SurfaceInfo::SurfaceInfo(
    const gfx::AcceleratedWidget& native_widget,
    const gfx::GLSurfaceHandle& handle)
    : native_widget(native_widget), handle(handle) {}

GpuSurfaceTracker::SurfaceInfo::~SurfaceInfo() { }


}  // namespace content
