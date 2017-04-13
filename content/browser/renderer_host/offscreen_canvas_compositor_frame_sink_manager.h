// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_observer.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

namespace content {

class CONTENT_EXPORT OffscreenCanvasCompositorFrameSinkManager
    : public cc::SurfaceObserver {
 public:
  OffscreenCanvasCompositorFrameSinkManager();
  virtual ~OffscreenCanvasCompositorFrameSinkManager();

  static OffscreenCanvasCompositorFrameSinkManager* GetInstance();

  void RegisterOffscreenCanvasSurfaceInstance(
      const cc::FrameSinkId& frame_sink_id,
      OffscreenCanvasSurfaceImpl* offscreen_canvas_surface);
  void UnregisterOffscreenCanvasSurfaceInstance(
      const cc::FrameSinkId& frame_sink_id);
  OffscreenCanvasSurfaceImpl* GetSurfaceInstance(
      const cc::FrameSinkId& frame_sink_id);

 private:
  friend class OffscreenCanvasCompositorFrameSinkManagerTest;

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;
  void OnSurfaceDamaged(const cc::SurfaceId&, bool* changed) override {}

  // When an OffscreenCanvasSurfaceImpl instance is destructed, it will
  // unregister the corresponding entry from this map.
  std::unordered_map<cc::FrameSinkId,
                     OffscreenCanvasSurfaceImpl*,
                     cc::FrameSinkIdHash>
      registered_surface_instances_;
  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasCompositorFrameSinkManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OFFSCREEN_CANVAS_COMPOSITOR_FRAME_SINK_MANAGER_H_
