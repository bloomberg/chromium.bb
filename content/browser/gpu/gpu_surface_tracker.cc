// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_surface_tracker.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#include "content/browser/android/child_process_launcher_android.h"
#include "ui/gl/android/scoped_java_surface.h"
#endif  // defined(OS_ANDROID)

namespace content {

GpuSurfaceTracker::GpuSurfaceTracker()
    : next_surface_id_(1) {
  gpu::GpuSurfaceLookup::InitInstance(this);
}

GpuSurfaceTracker::~GpuSurfaceTracker() {
  gpu::GpuSurfaceLookup::InitInstance(NULL);
}

GpuSurfaceTracker* GpuSurfaceTracker::GetInstance() {
  return base::Singleton<GpuSurfaceTracker>::get();
}

int GpuSurfaceTracker::AddSurfaceForNativeWidget(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] = widget;
  return surface_id;
}

void GpuSurfaceTracker::RemoveSurface(int surface_id) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  surface_map_.erase(surface_id);
}

gpu::SurfaceHandle GpuSurfaceTracker::GetSurfaceHandle(int surface_id) {
  DCHECK(surface_id);
#if defined(OS_MACOSX) || defined(OS_ANDROID)
#if DCHECK_IS_ON()
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
#endif
  // On Mac and Android, we can't pass the AcceleratedWidget, which is
  // process-local, so instead we pass the surface_id, so that we can look up
  // the AcceleratedWidget on the GPU side or when we receive
  // GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params.
  return surface_id;
#else
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  return it->second;
#endif
}

gfx::AcceleratedWidget GpuSurfaceTracker::AcquireNativeWidget(int surface_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return gfx::kNullAcceleratedWidget;

#if defined(OS_ANDROID)
  if (it->second != gfx::kNullAcceleratedWidget)
    ANativeWindow_acquire(it->second);
#endif  // defined(OS_ANDROID)

  return it->second;
}

#if defined(OS_ANDROID)
gfx::ScopedJavaSurface GpuSurfaceTracker::AcquireJavaSurface(int surface_id) {
  return GetViewSurface(surface_id);
}
#endif

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(lock_);
  return surface_map_.size();
}

}  // namespace content
