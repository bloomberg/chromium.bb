// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_H_
#define CC_SURFACES_DISPLAY_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/renderer.h"
#include "cc/resources/returned_resource.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surfaces_export.h"

namespace gfx {
class Size;
}

namespace cc {

class BlockingTaskRunner;
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
class CC_SURFACES_EXPORT Display : public OutputSurfaceClient,
                                   public RendererClient,
                                   public SurfaceDamageObserver {
 public:
  Display(DisplayClient* client,
          SurfaceManager* manager,
          SharedBitmapManager* bitmap_manager,
          gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
          const RendererSettings& settings);
  ~Display() override;

  bool Initialize(scoped_ptr<OutputSurface> output_surface);

  // device_scale_factor is used to communicate to the external window system
  // what scale this was rendered at.
  void SetSurfaceId(SurfaceId id, float device_scale_factor);
  void Resize(const gfx::Size& new_size);
  bool Draw();

  SurfaceId CurrentSurfaceId();
  int GetMaxFramesPending();

  // OutputSurfaceClient implementation.
  void DeferredInitialize() override {}
  void ReleaseGL() override {}
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override {}
  void DidSwapBuffers() override;
  void DidSwapBuffersComplete() override;
  void ReclaimResources(const CompositorFrameAck* ack) override {}
  void DidLoseOutputSurface() override;
  void SetExternalDrawConstraints(
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority,
      bool resourceless_software_draw) override {}
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy) override;
  void SetTreeActivationCallback(const base::Closure& callback) override {}

  // RendererClient implementation.
  void SetFullRootLayerDamage() override {}

  // SurfaceDamageObserver implementation.
  void OnSurfaceDamaged(SurfaceId surface, bool* changed) override;

 private:
  void InitializeRenderer();

  DisplayClient* client_;
  SurfaceManager* manager_;
  SharedBitmapManager* bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  RendererSettings settings_;
  SurfaceId current_surface_id_;
  gfx::Size current_surface_size_;
  float device_scale_factor_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<SurfaceAggregator> aggregator_;
  scoped_ptr<DirectRenderer> renderer_;
  scoped_ptr<BlockingTaskRunner> blocking_main_thread_task_runner_;
  scoped_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;
  std::vector<ui::LatencyInfo> stored_latency_info_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_H_
