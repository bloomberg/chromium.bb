// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_VIEW_H_
#define ASH_FAST_INK_FAST_INK_VIEW_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/output/layer_tree_frame_sink_client.h"
#include "components/viz/common/resources/resource_id.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace views {
class Widget;
}

namespace ash {

// FastInkView is a view supporting low-latency rendering.
class FastInkView : public views::View, public cc::LayerTreeFrameSinkClient {
 public:
  // Creates a FastInkView filling the bounds of |root_window|.
  // If |root_window| is resized (e.g. due to a screen size change),
  // a new instance of FastInkView should be created.
  explicit FastInkView(aura::Window* root_window);
  ~FastInkView() override;

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void SetTreeActivationCallback(const base::Closure& callback) override {}
  void DidReceiveCompositorFrameAck() override;
  void DidLoseLayerTreeFrameSink() override {}
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override {}

 protected:
  // Unions |rect| with the current damage rect.
  void UpdateDamageRect(const gfx::Rect& rect);

  void RequestRedraw();

  // Draw the contents of the view in the provided canvas.
  virtual void OnRedraw(gfx::Canvas& canvas) = 0;

 private:
  struct Resource;

  void UpdateBuffer();
  void UpdateSurface();
  void OnDidDrawSurface();

  std::unique_ptr<views::Widget> widget_;
  gfx::Transform screen_to_buffer_transform_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gfx::Rect buffer_damage_rect_;
  bool pending_update_buffer_ = false;
  gfx::Rect surface_damage_rect_;
  bool needs_update_surface_ = false;
  bool pending_draw_surface_ = false;
  int next_resource_id_ = 1;
  base::flat_map<viz::ResourceId, std::unique_ptr<Resource>>
      exported_resources_;
  std::vector<std::unique_ptr<Resource>> returned_resources_;
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;
  base::WeakPtrFactory<FastInkView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FastInkView);
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_VIEW_H_
