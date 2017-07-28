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
#include "components/viz/common/resources/resource_id.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace cc {
struct ReturnedResource;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace views {
class Widget;
}

namespace ash {
class FastInkLayerTreeFrameSinkHolder;
struct FastInkResource;

// FastInkView is a view supporting low-latency rendering.
class FastInkView : public views::View {
 public:
  // Creates a FastInkView filling the bounds of |root_window|.
  // If |root_window| is resized (e.g. due to a screen size change),
  // a new instance of FastInkView should be created.
  explicit FastInkView(aura::Window* root_window);
  ~FastInkView() override;

 protected:
  // Unions |rect| with the current damage rect.
  void UpdateDamageRect(const gfx::Rect& rect);

  void RequestRedraw();

  // Draw the contents of the view in the provided canvas.
  virtual void OnRedraw(gfx::Canvas& canvas, const gfx::Vector2d& offset) = 0;

 private:
  friend class FastInkLayerTreeFrameSinkHolder;

  // Call this to indicate that the previous frame has been processed.
  void DidReceiveCompositorFrameAck();

  // Call this to return resources so they can be reused or freed.
  void ReclaimResources(const std::vector<cc::ReturnedResource>& resources);

  void UpdateBuffer();
  void UpdateSurface();
  void OnDidDrawSurface();

  std::unique_ptr<views::Widget> widget_;
  float scale_factor_ = 1.0f;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gfx::Rect buffer_damage_rect_;
  bool pending_update_buffer_ = false;
  gfx::Rect surface_damage_rect_;
  bool needs_update_surface_ = false;
  bool pending_draw_surface_ = false;
  std::unique_ptr<FastInkLayerTreeFrameSinkHolder> frame_sink_holder_;
  int next_resource_id_ = 1;
  base::flat_map<viz::ResourceId, std::unique_ptr<FastInkResource>> resources_;
  std::vector<std::unique_ptr<FastInkResource>> returned_resources_;
  base::WeakPtrFactory<FastInkView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FastInkView);
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_VIEW_H_
