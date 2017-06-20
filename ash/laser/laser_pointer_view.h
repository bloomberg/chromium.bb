// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_VIEW_H_
#define ASH_LASER_LASER_POINTER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/laser/laser_pointer_points.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace gfx {
class GpuMemoryBuffer;
class PointF;
}

namespace views {
class Widget;
}

namespace ash {
class LaserLayerTreeFrameSinkHolder;
struct LaserResource;

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public views::View {
 public:
  LaserPointerView(base::TimeDelta life_duration,
                   base::TimeDelta presentation_delay,
                   aura::Window* root_window);
  ~LaserPointerView() override;

  void AddNewPoint(const gfx::PointF& new_point,
                   const base::TimeTicks& new_time);
  void UpdateTime();
  void Stop();

  // Call this to indicate that the previous frame has been processed.
  void DidReceiveCompositorFrameAck();

  // Call this to return resources so they can be reused or freed.
  void ReclaimResources(const cc::ReturnedResourceArray& resources);

 private:
  friend class LaserPointerControllerTestApi;

  gfx::Rect GetBoundingBox();
  void OnPointsUpdated();
  void UpdateBuffer();
  void OnBufferUpdated();
  void UpdateSurface();
  void OnDidDrawSurface();

  LaserPointerPoints laser_points_;
  LaserPointerPoints predicted_laser_points_;
  std::unique_ptr<views::Widget> widget_;
  float scale_factor_ = 1.0f;
  const base::TimeDelta presentation_delay_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gfx::Rect buffer_damage_rect_;
  bool pending_update_buffer_ = false;
  gfx::Rect surface_damage_rect_;
  bool needs_update_surface_ = false;
  bool pending_draw_surface_ = false;
  std::unique_ptr<LaserLayerTreeFrameSinkHolder> frame_sink_holder_;
  int next_resource_id_ = 1;
  base::flat_map<int, std::unique_ptr<LaserResource>> resources_;
  std::vector<std::unique_ptr<LaserResource>> returned_resources_;
  base::WeakPtrFactory<LaserPointerView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerView);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_VIEW_H_
