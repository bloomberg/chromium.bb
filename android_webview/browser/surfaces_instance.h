// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SURFACES_INSTANCE_H_
#define ANDROID_WEBVIEW_BROWSER_SURFACES_INSTANCE_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"

namespace cc {
class BeginFrameSource;
}

namespace gfx {
class Rect;
class Size;
class Transform;
}

namespace viz {
class CompositorFrameSinkSupport;
class Display;
class FrameSinkManagerImpl;
class LocalSurfaceIdAllocator;
}  // namespace viz

namespace android_webview {

class ParentOutputSurface;

class SurfacesInstance : public base::RefCounted<SurfacesInstance>,
                         public viz::DisplayClient,
                         public viz::CompositorFrameSinkSupportClient {
 public:
  static scoped_refptr<SurfacesInstance> GetOrCreateInstance();

  viz::FrameSinkId AllocateFrameSinkId();
  viz::FrameSinkManagerImpl* GetFrameSinkManager();

  void DrawAndSwap(const gfx::Size& viewport,
                   const gfx::Rect& clip,
                   const gfx::Transform& transform,
                   const gfx::Size& frame_size,
                   const viz::SurfaceId& child_id);

  void AddChildId(const viz::SurfaceId& child_id);
  void RemoveChildId(const viz::SurfaceId& child_id);

 private:
  friend class base::RefCounted<SurfacesInstance>;

  SurfacesInstance();
  ~SurfacesInstance() override;

  // viz::DisplayClient overrides.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const cc::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

  // viz::CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void WillDrawSurface(const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;
  void OnBeginFramePausedChanged(bool paused) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;

  void SetSolidColorRootFrame();

  viz::FrameSinkIdAllocator frame_sink_id_allocator_;

  viz::FrameSinkId frame_sink_id_;

  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<cc::BeginFrameSource> begin_frame_source_;
  std::unique_ptr<viz::Display> display_;
  std::unique_ptr<viz::LocalSurfaceIdAllocator> local_surface_id_allocator_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  viz::LocalSurfaceId root_id_;
  std::vector<viz::SurfaceId> child_ids_;

  // This is owned by |display_|.
  ParentOutputSurface* output_surface_;

  gfx::Size surface_size_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesInstance);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SURFACES_INSTANCE_H_
