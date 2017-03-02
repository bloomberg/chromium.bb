// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LASER_LASER_POINTER_VIEW_H_
#define ASH_LASER_LASER_POINTER_VIEW_H_

#include <deque>
#include <memory>
#include <unordered_map>

#include "ash/laser/laser_pointer_points.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace gfx {
class GpuMemoryBuffer;
class Point;
}

namespace views {
class Widget;
}

namespace ash {
struct LaserResource;

// LaserPointerView displays the palette tool laser pointer. It draws the laser,
// which consists of a point where the mouse cursor should be, as well as a
// trail of lines to help users track.
class LaserPointerView : public views::View,
                         public cc::mojom::MojoCompositorFrameSink,
                         public cc::CompositorFrameSinkSupportClient {
 public:
  LaserPointerView(base::TimeDelta life_duration, aura::Window* root_window);
  ~LaserPointerView() override;

  void AddNewPoint(const gfx::Point& new_point);
  void UpdateTime();
  void Stop();

  // Overridden from cc::mojom::MojoCompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(const cc::LocalSurfaceId& local_surface_id,
                             cc::CompositorFrame frame) override;
  void EvictFrame() override;

  // Overridden from cc::CompositorFrameSinkSupportClient:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override {}
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override {}

 private:
  friend class LaserPointerControllerTestApi;

  gfx::Rect GetBoundingBox();
  void OnPointsUpdated();
  void UpdateBuffer();
  void OnBufferUpdated();
  void UpdateSurface();
  void OnDidDrawSurface();

  LaserPointerPoints laser_points_;
  std::unique_ptr<views::Widget> widget_;
  float scale_factor_ = 1.0f;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gfx::Rect buffer_damage_rect_;
  bool pending_update_buffer_ = false;
  gfx::Rect surface_damage_rect_;
  bool needs_update_surface_ = false;
  bool pending_draw_surface_ = false;
  const cc::FrameSinkId frame_sink_id_;
  cc::CompositorFrameSinkSupport frame_sink_support_;
  cc::LocalSurfaceId local_surface_id_;
  cc::LocalSurfaceIdAllocator id_allocator_;
  int next_resource_id_ = 1;
  std::unordered_map<int, std::unique_ptr<LaserResource>> resources_;
  std::deque<std::unique_ptr<LaserResource>> returned_resources_;
  base::WeakPtrFactory<LaserPointerView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LaserPointerView);
};

}  // namespace ash

#endif  // ASH_LASER_LASER_POINTER_VIEW_H_
