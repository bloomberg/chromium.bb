// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_

#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_observer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/primary_begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace cc {

class BeginFrameSource;

namespace test {
class SurfaceSynchronizationTest;
}

}  // namespace cc

namespace viz {

class DisplayProvider;
class FrameSinkManagerClient;

// FrameSinkManagerImpl manages BeginFrame hierarchy. This is the implementation
// detail for FrameSinkManagerImpl.
class VIZ_SERVICE_EXPORT FrameSinkManagerImpl
    : public cc::SurfaceObserver,
      public NON_EXPORTED_BASE(cc::mojom::FrameSinkManager) {
 public:
  FrameSinkManagerImpl(DisplayProvider* display_provider = nullptr,
                       cc::SurfaceManager::LifetimeType lifetime_type =
                           cc::SurfaceManager::LifetimeType::SEQUENCES);
  ~FrameSinkManagerImpl() override;

  // Binds |this| as a FrameSinkManagerImpl for |request| on |task_runner|. On
  // Mac |task_runner| will be the resize helper task runner. May only be called
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

  // CompositorFrameSinkSupport, hierarchy, and BeginFrameSource can be
  // registered and unregistered in any order with respect to each other.
  //
  // This happens in practice, e.g. the relationship to between ui::Compositor /
  // DelegatedFrameHost is known before ui::Compositor has a surface/client).
  // However, DelegatedFrameHost can register itself as a client before its
  // relationship with the ui::Compositor is known.

  // Associates a FrameSinkManagerClient with the frame_sink_id it uses.
  // FrameSinkManagerClient and framesink allocators have a 1:1 mapping.
  // Caller guarantees the client is alive between register/unregister.
  void RegisterFrameSinkManagerClient(const FrameSinkId& frame_sink_id,
                                      FrameSinkManagerClient* client);
  void UnregisterFrameSinkManagerClient(const FrameSinkId& frame_sink_id);

  // Associates a |source| with a particular framesink.  That framesink and
  // any children of that framesink with valid clients can potentially use
  // that |source|.
  void RegisterBeginFrameSource(cc::BeginFrameSource* source,
                                const FrameSinkId& frame_sink_id);
  void UnregisterBeginFrameSource(cc::BeginFrameSource* source);

  // Returns a stable BeginFrameSource that forwards BeginFrames from the first
  // available BeginFrameSource.
  cc::BeginFrameSource* GetPrimaryBeginFrameSource();

  cc::SurfaceManager* surface_manager() { return &surface_manager_; }

  // cc::SurfaceObserver implementation.
  void OnSurfaceCreated(const SurfaceInfo& surface_info) override;
  bool OnSurfaceDamaged(const SurfaceId& surface_id,
                        const cc::BeginFrameAck& ack) override;
  void OnSurfaceDiscarded(const SurfaceId& surface_id) override;
  void OnSurfaceDestroyed(const SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const SurfaceId& surface_id,
                               const cc::BeginFrameArgs& args) override;
  void OnSurfaceWillDraw(const SurfaceId& surface_id) override;

  void OnClientConnectionLost(const FrameSinkId& frame_sink_id);
  void OnPrivateConnectionLost(const FrameSinkId& frame_sink_id);

  // It is necessary to pass |frame_sink_id| by value because the id
  // is owned by the GpuCompositorFrameSink in the map. When the sink is
  // removed from the map, |frame_sink_id| would also be destroyed if it were a
  // reference. But the map can continue to iterate and try to use it. Passing
  // by value avoids this.
  void DestroyCompositorFrameSink(FrameSinkId frame_sink_id);

 private:
  friend class cc::test::SurfaceSynchronizationTest;

  void RecursivelyAttachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         cc::BeginFrameSource* source);
  void RecursivelyDetachBeginFrameSource(const FrameSinkId& frame_sink_id,
                                         cc::BeginFrameSource* source);

  // Returns true if |child framesink| is or has |search_frame_sink_id| as a
  // child.
  bool ChildContains(const FrameSinkId& child_frame_sink_id,
                     const FrameSinkId& search_frame_sink_id) const;

  // Begin frame source routing. Both BeginFrameSource and
  // CompositorFrameSinkSupport pointers guaranteed alive by callers until
  // unregistered.
  struct FrameSinkSourceMapping {
    FrameSinkSourceMapping();
    FrameSinkSourceMapping(const FrameSinkSourceMapping& other);
    ~FrameSinkSourceMapping();
    bool has_children() const { return !children.empty(); }
    // The currently assigned begin frame source for this client.
    cc::BeginFrameSource* source = nullptr;
    // This represents a dag of parent -> children mapping.
    std::vector<FrameSinkId> children;
  };

  // Provides a Display for CreateRootCompositorFrameSink().
  DisplayProvider* const display_provider_;

  base::flat_map<FrameSinkId, FrameSinkManagerClient*> clients_;
  std::unordered_map<FrameSinkId, FrameSinkSourceMapping, FrameSinkIdHash>
      frame_sink_source_map_;

  // Set of BeginFrameSource along with associated FrameSinkIds. Any child
  // that is implicitly using this framesink must be reachable by the
  // parent in the dag.
  std::unordered_map<cc::BeginFrameSource*, FrameSinkId> registered_sources_;

  PrimaryBeginFrameSource primary_source_;

  // |surface_manager_| should be placed under |primary_source_| so that all
  // surfaces are destroyed before |primary_source_|.
  cc::SurfaceManager surface_manager_;

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

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_MANAGER_IMPL_H_
