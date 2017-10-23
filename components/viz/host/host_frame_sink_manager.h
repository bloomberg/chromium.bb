// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "components/viz/host/viz_host_export.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {

class CompositorFrameSinkSupport;
class FrameSinkManagerImpl;
class SurfaceInfo;


// Browser side wrapper of mojom::FrameSinkManager, to be used from the
// UI thread. Manages frame sinks and is intended to replace all usage of
// FrameSinkManagerImpl.
class VIZ_HOST_EXPORT HostFrameSinkManager
    : public mojom::FrameSinkManagerClient,
      public CompositorFrameSinkSupportManager {
 public:
  HostFrameSinkManager();
  ~HostFrameSinkManager() override;

  using DisplayHitTestQueryMap =
      base::flat_map<FrameSinkId, std::unique_ptr<HitTestQuery>>;
  const DisplayHitTestQueryMap& display_hit_test_query() const {
    return display_hit_test_query_;
  }

  // Sets a local FrameSinkManagerImpl instance and connects directly to it.
  void SetLocalManager(FrameSinkManagerImpl* frame_sink_manager_impl);

  // Binds |this| as a FrameSinkManagerClient for |request| on |task_runner|. On
  // Mac |task_runner| will be the resize helper task runner. May only be called
  // once. If |task_runner| is null, it uses the default mojo task runner for
  // the thread this call is made on.
  void BindAndSetManager(
      mojom::FrameSinkManagerClientRequest request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojom::FrameSinkManagerPtr ptr);

  // Sets a callback to be notified when the connection to the FrameSinkManager
  // on |frame_sink_manager_ptr_| is lost.
  void SetConnectionLostCallback(base::RepeatingClosure callback);

  // Registers |frame_sink_id| will be used. This must be called before
  // CreateCompositorFrameSink(Support) is called.
  void RegisterFrameSinkId(const FrameSinkId& frame_sink_id,
                           HostFrameSinkClient* client);

  // Invalidates |frame_sink_id| which cleans up any unsatisified surface
  // sequences or dangling temporary references assigned to it. If there is a
  // CompositorFrameSink for |frame_sink_id| then it will be destroyed and the
  // message pipe to the client will be closed.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id);

  // |debug_label| is used when printing out the surface hierarchy so we know
  // which clients are contributing which surfaces.
  void SetFrameSinkDebugLabel(const FrameSinkId& frame_sink_id,
                              const std::string& debug_label);

  // Creates a connection for a display root to viz. Provides the same
  // interfaces as CreateCompositorFramesink() plus the priviledged
  // DisplayPrivate interface. When no longer needed, call
  // InvalidateFrameSinkId().
  void CreateRootCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      const RendererSettings& renderer_settings,
      mojom::CompositorFrameSinkAssociatedRequest request,
      mojom::CompositorFrameSinkClientPtr client,
      mojom::DisplayPrivateAssociatedRequest display_private_request);

  // Creates a connection between client to viz, using |request| and |client|,
  // that allows the client to submit CompositorFrames. When no longer needed,
  // call InvalidateFrameSinkId().
  void CreateCompositorFrameSink(const FrameSinkId& frame_sink_id,
                                 mojom::CompositorFrameSinkRequest request,
                                 mojom::CompositorFrameSinkClientPtr client);

  // Registers frame sink hierarchy. A frame sink can have multiple parents.
  void RegisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                  const FrameSinkId& child_frame_sink_id);

  // Unregisters FrameSink hierarchy. Client must have registered frame sink
  // hierarchy before unregistering.
  void UnregisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                    const FrameSinkId& child_frame_sink_id);

  // These two functions should only be used by WindowServer.
  // TODO(riajiang): Find a better way for HostFrameSinkManager to do the assign
  // and drop instead.
  void AssignTemporaryReference(const SurfaceId& surface_id,
                                const FrameSinkId& owner);
  void DropTemporaryReference(const SurfaceId& surface_id);

  // CompositorFrameSinkSupportManager:
  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      mojom::CompositorFrameSinkClient* client,
      const FrameSinkId& frame_sink_id,
      bool is_root,
      bool needs_sync_points) override;

 private:
  friend class HostFrameSinkManagerTestBase;

  struct FrameSinkData {
    FrameSinkData();
    FrameSinkData(FrameSinkData&& other);
    ~FrameSinkData();
    FrameSinkData& operator=(FrameSinkData&& other);

    bool IsFrameSinkRegistered() const { return client != nullptr; }

    bool HasCompositorFrameSinkData() const {
      return has_created_compositor_frame_sink || support;
    }

    // Returns true if there is nothing in FrameSinkData and it can be deleted.
    bool IsEmpty() const {
      return !IsFrameSinkRegistered() && !HasCompositorFrameSinkData() &&
             parents.empty() && children.empty();
    }

    // The client to be notified of changes to this FrameSink.
    HostFrameSinkClient* client = nullptr;

    // If the frame sink is a root that corresponds to a Display.
    bool is_root = false;

    // If a mojom::CompositorFrameSink was created for this FrameSinkId. This
    // will always be false if not using Mojo.
    bool has_created_compositor_frame_sink = false;

    // This will be null if using Mojo.
    CompositorFrameSinkSupport* support = nullptr;

    // Track frame sink hierarchy in both directions.
    std::vector<FrameSinkId> parents;
    std::vector<FrameSinkId> children;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameSinkData);
  };

  // Provided as a callback to clear state when a CompositorFrameSinkSupport is
  // destroyed.
  void CompositorFrameSinkSupportDestroyed(const FrameSinkId& frame_sink_id);

  // Assigns the temporary reference to the frame sink that is expected to
  // embeded |surface_id|, otherwise drops the temporary reference.
  void PerformAssignTemporaryReference(const SurfaceId& surface_id);

  // Handles connection loss to |frame_sink_manager_ptr_|. This should only
  // happen when the GPU process crashes.
  void OnConnectionLost();

  // Registers FrameSinkId and FrameSink hierarchy again after connection loss.
  void RegisterAfterConnectionLoss();

  // mojom::FrameSinkManagerClient:
  void OnFirstSurfaceActivation(const SurfaceInfo& surface_info) override;
  void OnClientConnectionClosed(const FrameSinkId& frame_sink_id) override;
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_sizes) override;
  void SwitchActiveAggregatedHitTestRegionList(
      const FrameSinkId& frame_sink_id,
      uint8_t active_handle_index) override;

  // This will point to |frame_sink_manager_ptr_| if using Mojo or
  // |frame_sink_manager_impl_| if directly connected. Use this to make function
  // calls.
  mojom::FrameSinkManager* frame_sink_manager_ = nullptr;

  // Mojo connection to the FrameSinkManager. If this is bound then
  // |frame_sink_manager_impl_| must be null.
  mojom::FrameSinkManagerPtr frame_sink_manager_ptr_;

  // Mojo connection back from the FrameSinkManager.
  mojo::Binding<mojom::FrameSinkManagerClient> binding_;

  // A direct connection to FrameSinkManagerImpl. If this is set then
  // |frame_sink_manager_ptr_| must be unbound. For use in browser process only,
  // viz process should not set this.
  FrameSinkManagerImpl* frame_sink_manager_impl_ = nullptr;

  // Per CompositorFrameSink data.
  base::flat_map<FrameSinkId, FrameSinkData> frame_sink_data_map_;

  // If |frame_sink_manager_ptr_| connection was lost.
  bool connection_was_lost_ = false;

  base::RepeatingClosure connection_lost_callback_;

  DisplayHitTestQueryMap display_hit_test_query_;

  base::WeakPtrFactory<HostFrameSinkManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
