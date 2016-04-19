// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_H_
#define CC_SURFACES_DISPLAY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/renderer.h"
#include "cc/resources/returned_resource.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/events/latency_info.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace gfx {
class Size;
}

namespace cc {

class BeginFrameSource;
class DirectRenderer;
class DisplayClient;
class OutputSurface;
class RendererSettings;
class ResourceProvider;
class SharedBitmapManager;
class Surface;
class SurfaceAggregator;
class SurfaceIdAllocator;
class SurfaceFactory;
class TextureMailboxDeleter;

// A Display produces a surface that can be used to draw to a physical display
// (OutputSurface). The client is responsible for creating and sizing the
// surface IDs used to draw into the display and deciding when to draw.
class CC_SURFACES_EXPORT Display : public DisplaySchedulerClient,
                                   public OutputSurfaceClient,
                                   public RendererClient,
                                   public SurfaceDamageObserver {
 public:
  Display(DisplayClient* client,
          SurfaceManager* manager,
          SharedBitmapManager* bitmap_manager,
          gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
          const RendererSettings& settings,
          uint32_t compositor_surface_namespace);
  ~Display() override;

  bool Initialize(std::unique_ptr<OutputSurface> output_surface,
                  base::SingleThreadTaskRunner* task_runner);

  // device_scale_factor is used to communicate to the external window system
  // what scale this was rendered at.
  void SetSurfaceId(SurfaceId id, float device_scale_factor);
  void Resize(const gfx::Size& new_size);
  void SetExternalClip(const gfx::Rect& clip);

  SurfaceId CurrentSurfaceId();

  // DisplaySchedulerClient implementation.
  bool DrawAndSwap() override;

  // OutputSurfaceClient implementation.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override {}
  void SetBeginFrameSource(BeginFrameSource* source) override;
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void DidSwapBuffers() override;
  void DidSwapBuffersComplete() override;
  void ReclaimResources(const CompositorFrameAck* ack) override;
  void DidLoseOutputSurface() override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              const gfx::Rect& clip,
              bool resourceless_software_draw) override;

  // RendererClient implementation.
  void SetFullRootLayerDamage() override;

  // SurfaceDamageObserver implementation.
  void OnSurfaceDamaged(SurfaceId surface, bool* changed) override;

 protected:
  // Virtual for tests.
  virtual void CreateScheduler(base::SingleThreadTaskRunner* task_runner);

  void InitializeRenderer();
  void UpdateRootSurfaceResourcesLocked();

  DisplayClient* client_;
  SurfaceManager* surface_manager_;
  SharedBitmapManager* bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  RendererSettings settings_;
  SurfaceId current_surface_id_;
  uint32_t compositor_surface_namespace_;
  gfx::Size current_surface_size_;
  float device_scale_factor_;
  bool swapped_since_resize_;
  gfx::Rect external_clip_;
  std::unique_ptr<OutputSurface> output_surface_;
  // An internal synthetic BFS. May be null when not used.
  std::unique_ptr<BeginFrameSource> internal_begin_frame_source_;
  // The real BFS tied to vsync provided by the BrowserCompositorOutputSurface.
  BeginFrameSource* vsync_begin_frame_source_;
  // The current BFS driving the Display/DisplayScheduler.
  BeginFrameSource* observed_begin_frame_source_;
  std::unique_ptr<DisplayScheduler> scheduler_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  std::unique_ptr<DirectRenderer> renderer_;
  std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;
  std::vector<ui::LatencyInfo> stored_latency_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_H_
