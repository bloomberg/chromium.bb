// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/display_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"
#include "ui/latency/latency_info.h"

namespace cc {
class DirectRenderer;
class DisplayResourceProvider;
class OutputSurface;
class RendererSettings;
class SoftwareRenderer;
}  // namespace cc

namespace gpu {
class GpuMemoryBufferManager;
}

namespace gfx {
class Size;
}

namespace viz {

class DisplayClient;
class SharedBitmapManager;
class TextureMailboxDeleter;

class VIZ_SERVICE_EXPORT DisplayObserver {
 public:
  virtual ~DisplayObserver() {}

  virtual void OnDisplayDidFinishFrame(const BeginFrameAck& ack) = 0;
};

// A Display produces a surface that can be used to draw to a physical display
// (OutputSurface). The client is responsible for creating and sizing the
// surface IDs used to draw into the display and deciding when to draw.
class VIZ_SERVICE_EXPORT Display : public DisplaySchedulerClient,
                                   public cc::OutputSurfaceClient {
 public:
  // The |begin_frame_source| and |scheduler| may be null (together). In that
  // case, DrawAndSwap must be called externally when needed.
  Display(SharedBitmapManager* bitmap_manager,
          gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
          const RendererSettings& settings,
          const FrameSinkId& frame_sink_id,
          std::unique_ptr<cc::OutputSurface> output_surface,
          std::unique_ptr<DisplayScheduler> scheduler,
          std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter);

  ~Display() override;

  void Initialize(DisplayClient* client, SurfaceManager* surface_manager);

  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  // device_scale_factor is used to communicate to the external window system
  // what scale this was rendered at.
  void SetLocalSurfaceId(const LocalSurfaceId& id, float device_scale_factor);
  void SetVisible(bool visible);
  void Resize(const gfx::Size& new_size);
  void SetColorSpace(const gfx::ColorSpace& blending_color_space,
                     const gfx::ColorSpace& device_color_space);
  void SetOutputIsSecure(bool secure);

  const SurfaceId& CurrentSurfaceId();

  // DisplaySchedulerClient implementation.
  bool DrawAndSwap() override;
  bool SurfaceHasUndrawnFrame(const SurfaceId& surface_id) const override;
  bool SurfaceDamaged(const SurfaceId& surface_id,
                      const BeginFrameAck& ack) override;
  void SurfaceDiscarded(const SurfaceId& surface_id) override;
  void DidFinishFrame(const BeginFrameAck& ack) override;

  // OutputSurfaceClient implementation.
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void DidReceiveSwapBuffersAck() override;
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;

  bool has_scheduler() const { return !!scheduler_; }
  cc::DirectRenderer* renderer_for_testing() const { return renderer_.get(); }
  size_t stored_latency_info_size_for_testing() const {
    return stored_latency_info_.size();
  }

  void ForceImmediateDrawAndSwapIfPossible();

 private:
  void InitializeRenderer();
  void UpdateRootSurfaceResourcesLocked();
  void DidLoseContextProvider();

  SharedBitmapManager* const bitmap_manager_;
  gpu::GpuMemoryBufferManager* const gpu_memory_buffer_manager_;
  const RendererSettings settings_;

  DisplayClient* client_ = nullptr;
  base::ObserverList<DisplayObserver> observers_;
  SurfaceManager* surface_manager_ = nullptr;
  const FrameSinkId frame_sink_id_;
  SurfaceId current_surface_id_;
  gfx::Size current_surface_size_;
  float device_scale_factor_ = 1.f;
  gfx::ColorSpace blending_color_space_ = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace device_color_space_ = gfx::ColorSpace::CreateSRGB();
  bool visible_ = false;
  bool swapped_since_resize_ = false;
  bool output_is_secure_ = false;

  std::unique_ptr<cc::OutputSurface> output_surface_;
  std::unique_ptr<DisplayScheduler> scheduler_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  std::unique_ptr<TextureMailboxDeleter> texture_mailbox_deleter_;
  std::unique_ptr<cc::DirectRenderer> renderer_;
  cc::SoftwareRenderer* software_renderer_ = nullptr;
  std::vector<ui::LatencyInfo> stored_latency_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_
