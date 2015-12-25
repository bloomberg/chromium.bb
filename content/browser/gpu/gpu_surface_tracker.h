// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_SURFACE_TRACKER_H_
#define CONTENT_BROWSER_GPU_GPU_SURFACE_TRACKER_H_

#include <stddef.h>

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// This class is responsible for managing rendering surfaces exposed to the
// GPU process. Every surface gets registered to this class, and gets an ID.
// All calls to and from the GPU process, with the exception of
// CreateViewCommandBuffer, refer to the rendering surface by its ID.
// This class is thread safe.
//
// Note: The ID can exist before the actual native handle for the surface is
// created, for example to allow giving a reference to it to a renderer, so that
// it is unamibiguously identified.
class CONTENT_EXPORT GpuSurfaceTracker : public GpuSurfaceLookup {
 public:
  // GpuSurfaceLookup implementation:
  // Returns the native widget associated with a given surface_id.
  gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) override;

  // Gets the global instance of the surface tracker.
  static GpuSurfaceTracker* Get() { return GetInstance(); }

  // Adds a surface for a native widget. Returns the surface ID.
  int AddSurfaceForNativeWidget(gfx::AcceleratedWidget widget);

  // Removes a given existing surface.
  void RemoveSurface(int surface_id);

  // Sets the native handle for the given surface.
  // Note: This is an O(log N) lookup.
  void SetSurfaceHandle(int surface_id, const gfx::GLSurfaceHandle& handle);

  // Gets the native handle for the given surface.
  // Note: This is an O(log N) lookup.
  gfx::GLSurfaceHandle GetSurfaceHandle(int surface_id);

  // Returns the number of surfaces currently registered with the tracker.
  std::size_t GetSurfaceCount();

  // Gets the global instance of the surface tracker. Identical to Get(), but
  // named that way for the implementation of Singleton.
  static GpuSurfaceTracker* GetInstance();

 private:
  struct SurfaceInfo {
    SurfaceInfo();
    SurfaceInfo(const gfx::AcceleratedWidget& native_widget,
                const gfx::GLSurfaceHandle& handle);
    ~SurfaceInfo();
    gfx::AcceleratedWidget native_widget;
    gfx::GLSurfaceHandle handle;
  };
  typedef std::map<int, SurfaceInfo> SurfaceMap;

  friend struct base::DefaultSingletonTraits<GpuSurfaceTracker>;

  GpuSurfaceTracker();
  ~GpuSurfaceTracker() override;

  base::Lock lock_;
  SurfaceMap surface_map_;
  int next_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_SURFACE_TRACKER_H_
