// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_MOJO_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_MOJO_FRAME_SINK_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_observer.h"
#include "components/viz/service/frame_sinks/gpu_compositor_frame_sink_delegate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace viz {

class DisplayProvider;

// MojoFrameSinkManager manages state associated with CompositorFrameSinks. It
// provides a Mojo interface to create CompositorFrameSinks, manages BeginFrame
// hierarchy and manages surface lifetime.
//
// This is intended to be created in the viz or GPU process. For mus+ash this
// will be true after the mus process split. For non-mus Chrome this will be
// created in the browser process, at least until GPU implementations can be
// unified.
class VIZ_SERVICE_EXPORT MojoFrameSinkManager
    : public cc::SurfaceObserver,
      public NON_EXPORTED_BASE(GpuCompositorFrameSinkDelegate),
      public NON_EXPORTED_BASE(cc::mojom::FrameSinkManager) {
 public:
  MojoFrameSinkManager(bool use_surface_references,
                       DisplayProvider* display_provider);
  ~MojoFrameSinkManager() override;

  cc::SurfaceManager* surface_manager() { return &manager_; }

  // Binds |this| as a FrameSinkManager for a given |request|. This may
  // only be called once.
  void BindPtrAndSetClient(cc::mojom::FrameSinkManagerRequest request,
                           cc::mojom::FrameSinkManagerClientPtr client);

  // cc::mojom::FrameSinkManager implementation:
  void CreateRootCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override;
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client) override;
  void RegisterFrameSinkHierarchy(
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const cc::FrameSinkId& parent_frame_sink_id,
      const cc::FrameSinkId& child_frame_sink_id) override;
  void DropTemporaryReference(const cc::SurfaceId& surface_id) override;

 private:
  // It is necessary to pass |frame_sink_id| by value because the id
  // is owned by the GpuCompositorFrameSink in the map. When the sink is
  // removed from the map, |frame_sink_id| would also be destroyed if it were a
  // reference. But the map can continue to iterate and try to use it. Passing
  // by value avoids this.
  void DestroyCompositorFrameSink(cc::FrameSinkId frame_sink_id);

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;
  bool OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                        const cc::BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const cc::SurfaceId& surface_id) override;
  void OnSurfaceDestroyed(const cc::SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const cc::SurfaceId& surface_id,
                               const cc::BeginFrameArgs& args) override;
  void OnSurfaceWillDraw(const cc::SurfaceId& surface_id) override;

  // GpuCompositorFrameSinkDelegate implementation.
  void OnClientConnectionLost(const cc::FrameSinkId& frame_sink_id,
                              bool destroy_compositor_frame_sink) override;
  void OnPrivateConnectionLost(const cc::FrameSinkId& frame_sink_id,
                               bool destroy_compositor_frame_sink) override;

  // SurfaceManager should be the first object constructed and the last object
  // destroyed in order to ensure that all other objects that depend on it have
  // access to a valid pointer for the entirety of their lifetimes.
  cc::SurfaceManager manager_;

  std::unique_ptr<cc::SurfaceDependencyTracker> dependency_tracker_;

  // Provides a cc::Display for CreateRootCompositorFrameSink().
  DisplayProvider* const display_provider_;

  std::unordered_map<cc::FrameSinkId,
                     std::unique_ptr<cc::mojom::MojoCompositorFrameSink>,
                     cc::FrameSinkIdHash>
      compositor_frame_sinks_;

  THREAD_CHECKER(thread_checker_);

  cc::mojom::FrameSinkManagerClientPtr client_;
  mojo::Binding<cc::mojom::FrameSinkManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoFrameSinkManager);
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_FRAME_SINKS_MOJO_FRAME_SINK_MANAGER_H_
