// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "cc/surfaces/surface_observer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/frame_sink_manager.h"
#include "components/viz/service/frame_sinks/gpu_compositor_frame_sink_delegate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {

class DisplayProvider;

// FrameSinkManagerImpl manages state associated with CompositorFrameSinks. It
// provides a Mojo interface to create CompositorFrameSinks, manages BeginFrame
// hierarchy and manages surface lifetime.
//
// This is intended to be created in the viz or GPU process. For mus+ash this
// will be true after the mus process split. For non-mus Chrome this will be
// created in the browser process, at least until GPU implementations can be
// unified.
class VIZ_SERVICE_EXPORT FrameSinkManagerImpl
    : public cc::SurfaceObserver,
      public NON_EXPORTED_BASE(GpuCompositorFrameSinkDelegate),
      public NON_EXPORTED_BASE(cc::mojom::FrameSinkManager) {
 public:
  FrameSinkManagerImpl(bool use_surface_references,
                       DisplayProvider* display_provider);
  ~FrameSinkManagerImpl() override;

  viz::FrameSinkManager* frame_sink_manager() { return &manager_; }

  // Binds |this| as a FrameSinkManager for |request| on |task_runner|. On Mac
  // |task_runner| will be the resize helper task runner. May only be called
  // once.
  void BindAndSetClient(cc::mojom::FrameSinkManagerRequest request,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        cc::mojom::FrameSinkManagerClientPtr client);

  // Sets up a direction connection to |client| without using Mojo.
  void SetLocalClient(cc::mojom::FrameSinkManagerClient* client);

  // cc::mojom::FrameSinkManager implementation:
  void CreateRootCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::CompositorFrameSinkAssociatedRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override;
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      cc::mojom::CompositorFrameSinkRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client) override;
  void RegisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void DropTemporaryReference(const SurfaceId& surface_id) override;

 private:
  // It is necessary to pass |frame_sink_id| by value because the id
  // is owned by the GpuCompositorFrameSink in the map. When the sink is
  // removed from the map, |frame_sink_id| would also be destroyed if it were a
  // reference. But the map can continue to iterate and try to use it. Passing
  // by value avoids this.
  void DestroyCompositorFrameSink(FrameSinkId frame_sink_id);

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const SurfaceInfo& surface_info) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const cc::BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const cc::BeginFrameArgs& args) override;
  void OnSurfaceWillDraw(const SurfaceId& surface_id) override;

  // GpuCompositorFrameSinkDelegate implementation.
  void OnClientConnectionLost(const FrameSinkId& frame_sink_id) override;
  void OnPrivateConnectionLost(const FrameSinkId& frame_sink_id) override;

  // FrameSinkManager should be the first object constructed and the last object
  // destroyed in order to ensure that all other objects that depend on it have
  // access to a valid pointer for the entirety of their lifetimes.
  viz::FrameSinkManager manager_;

  // Provides a Display for CreateRootCompositorFrameSink().
  DisplayProvider* const display_provider_;

  std::unordered_map<FrameSinkId,
                     std::unique_ptr<cc::mojom::CompositorFrameSink>,
                     FrameSinkIdHash>
      compositor_frame_sinks_;

  THREAD_CHECKER(thread_checker_);

  // This will point to |client_ptr_| if using Mojo or a provided client if
  // directly connected. Use this to make function calls.
  cc::mojom::FrameSinkManagerClient* client_ = nullptr;

  cc::mojom::FrameSinkManagerClientPtr client_ptr_;
  mojo::Binding<cc::mojom::FrameSinkManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerImpl);
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
