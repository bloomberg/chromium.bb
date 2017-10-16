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

namespace gfx {
class GpuMemoryBuffer;
}

namespace views {
class Widget;
}

namespace ash {

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
  virtual void OnRedraw(gfx::Canvas& canvas) = 0;

 private:
  class LayerTreeFrameSinkHolder;
  struct Resource;

  void Redraw();
  void UpdateBuffer();
  void UpdateSurface();
  void ReclaimResource(std::unique_ptr<Resource> resource);
  void DidReceiveCompositorFrameAck();
  void OnDidDrawSurface();

  std::unique_ptr<views::Widget> widget_;
  gfx::Transform screen_to_buffer_transform_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gfx::Rect buffer_damage_rect_;
  bool pending_redraw_ = false;
  gfx::Rect surface_damage_rect_;
  bool pending_draw_surface_ = false;
  gfx::Rect pending_draw_surface_rect_;
  int next_resource_id_ = 1;
  std::vector<std::unique_ptr<Resource>> returned_resources_;
  std::unique_ptr<LayerTreeFrameSinkHolder> frame_sink_holder_;
  base::WeakPtrFactory<FastInkView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FastInkView);
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_VIEW_H_
