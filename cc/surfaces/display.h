// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_H_
#define CC_SURFACES_DISPLAY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/returned_resource.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surfaces_export.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/color_space.h"

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
class SoftwareRenderer;
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
                                   public SurfaceDamageObserver {
 public:
  // The |begin_frame_source| and |scheduler| may be null (together). In that
  // case, DrawAndSwap must be called externally when needed.
  Display(SharedBitmapManager* bitmap_manager,
          gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
          const RendererSettings& settings,
          std::unique_ptr<BeginFrameSource> begin_frame_source,
          std::unique_ptr<OutputSurface> output_surface,
          std::unique_ptr<DisplayScheduler> scheduler,
          std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter);

  ~Display() override;

  void Initialize(DisplayClient* client,
                  SurfaceManager* surface_manager,
                  uint32_t compositor_surface_namespace);

  // device_scale_factor is used to communicate to the external window system
  // what scale this was rendered at.
  void SetSurfaceId(const SurfaceId& id, float device_scale_factor);
  void SetVisible(bool visible);
  void Resize(const gfx::Size& new_size);
  void SetColorSpace(const gfx::ColorSpace& color_space);
  void SetExternalClip(const gfx::Rect& clip);
  void SetExternalViewport(const gfx::Rect& viewport);
  void SetOutputIsSecure(bool secure);

  const SurfaceId& CurrentSurfaceId();

  // DisplaySchedulerClient implementation.
  bool DrawAndSwap() override;

  // OutputSurfaceClient implementation.
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void SetBeginFrameSource(BeginFrameSource* source) override;
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void DidSwapBuffersComplete() override;
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;
  void ReclaimResources(const ReturnedResourceArray& resources) override;
  void DidLoseOutputSurface() override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override;

  // SurfaceDamageObserver implementation.
  void OnSurfaceDamaged(const SurfaceId& surface, bool* changed) override;

  bool has_scheduler() const { return !!scheduler_; }
  DirectRenderer* renderer_for_testing() const { return renderer_.get(); }

  void ForceImmediateDrawAndSwapIfPossible();

 private:
  void InitializeRenderer();
  void UpdateRootSurfaceResourcesLocked();

  SharedBitmapManager* const bitmap_manager_;
  gpu::GpuMemoryBufferManager* const gpu_memory_buffer_manager_;
  const RendererSettings settings_;

  DisplayClient* client_ = nullptr;
  SurfaceManager* surface_manager_ = nullptr;
  uint32_t compositor_surface_namespace_;
  SurfaceId current_surface_id_;
  gfx::Size current_surface_size_;
  float device_scale_factor_ = 1.f;
  gfx::ColorSpace device_color_space_;
  bool visible_ = false;
  bool swapped_since_resize_ = false;
  gfx::Rect external_clip_;
  gfx::Rect external_viewport_;
  bool output_is_secure_ = false;

  // The begin_frame_source_ is often known by the output_surface_ and
  // the scheduler_.
  std::unique_ptr<BeginFrameSource> begin_frame_source_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<DisplayScheduler> scheduler_;
  std::unique_ptr<ResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;
  std::unique_ptr<DirectRenderer> renderer_;
  SoftwareRenderer* software_renderer_ = nullptr;
  std::vector<ui::LatencyInfo> stored_latency_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_H_
