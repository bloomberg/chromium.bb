// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
#define COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/frame_sink_observer.h"
#include "components/viz/host/viz_host_export.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_manager.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class SurfaceInfo;
}  // namespace cc

namespace viz {

class CompositorFrameSinkSupport;
class CompositorFrameSinkSupportClient;
class FrameSinkManagerImpl;

namespace test {
class HostFrameSinkManagerTest;
}

// Browser side wrapper of mojom::FrameSinkManager, to be used from the
// UI thread. Manages frame sinks and is intended to replace SurfaceManager.
class VIZ_HOST_EXPORT HostFrameSinkManager
    : public NON_EXPORTED_BASE(cc::mojom::FrameSinkManagerClient),
      public NON_EXPORTED_BASE(CompositorFrameSinkSupportManager) {
 public:
  HostFrameSinkManager();
  ~HostFrameSinkManager() override;

  // Sets a local FrameSinkManagerImpl instance and connects directly to it.
  void SetLocalManager(FrameSinkManagerImpl* frame_sink_manager_impl);

  // Binds |this| as a FrameSinkManagerClient for |request| on |task_runner|. On
  // Mac |task_runner| will be the resize helper task runner. May only be called
  // once.
  void BindAndSetManager(
      cc::mojom::FrameSinkManagerClientRequest request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      cc::mojom::FrameSinkManagerPtr ptr);

  void AddObserver(FrameSinkObserver* observer);
  void RemoveObserver(FrameSinkObserver* observer);

  // Creates a connection between client to viz, using |request| and |client|,
  // that allows the client to submit CompositorFrames. When no longer needed,
  // call DestroyCompositorFrameSink().
  void CreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      cc::mojom::CompositorFrameSinkRequest request,
      cc::mojom::CompositorFrameSinkClientPtr client);

  // Destroys a client connection. Will call UnregisterFrameSinkHierarchy() with
  // the registered parent if there is one.
  void DestroyCompositorFrameSink(const FrameSinkId& frame_sink_id);

  // Registers FrameSink hierarchy. Clients can call this multiple times to
  // reparent without calling UnregisterFrameSinkHierarchy(). If a client uses
  // CompositorFrameSink, then CreateCompositorFrameSink() should be called
  // before this.
  void RegisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                  const FrameSinkId& child_frame_sink_id);

  // Unregisters FrameSink hierarchy. Client must have registered FrameSink
  // hierarchy before unregistering.
  void UnregisterFrameSinkHierarchy(const FrameSinkId& parent_frame_sink_id,
                                    const FrameSinkId& child_frame_sink_id);

  // CompositorFrameSinkSupportManager:
  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      CompositorFrameSinkSupportClient* client,
      const FrameSinkId& frame_sink_id,
      bool is_root,
      bool handles_frame_sink_id_invalidation,
      bool needs_sync_points) override;

 private:
  friend class test::HostFrameSinkManagerTest;

  struct FrameSinkData {
    FrameSinkData();
    FrameSinkData(FrameSinkData&& other);
    ~FrameSinkData();
    FrameSinkData& operator=(FrameSinkData&& other);

    // If the frame sink is a root that corresponds to a Display.
    bool is_root = false;

    // The FrameSinkId registered as the parent in the BeginFrame hierarchy.
    // This mirrors state in viz.
    base::Optional<FrameSinkId> parent;

    // The private interface that gives the host control over the
    // CompositorFrameSink connection between the client and viz. This will be
    // unbound if not using Mojo.
    cc::mojom::CompositorFrameSinkPrivatePtr private_interface;

    // This will be null if using Mojo.
    CompositorFrameSinkSupport* support = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(FrameSinkData);
  };

  // cc::mojom::FrameSinkManagerClient:
  void OnSurfaceCreated(const SurfaceInfo& surface_info) override;
  void OnClientConnectionClosed(const FrameSinkId& frame_sink_id) override;

  // This will point to |frame_sink_manager_ptr_| if using Mojo or
  // |frame_sink_manager_impl_| if directly connected. Use this to make function
  // calls.
  cc::mojom::FrameSinkManager* frame_sink_manager_ = nullptr;

  // Mojo connection to the FrameSinkManager. If this is bound then
  // |frame_sink_manager_impl_| must be null.
  cc::mojom::FrameSinkManagerPtr frame_sink_manager_ptr_;

  // Mojo connection back from the FrameSinkManager.
  mojo::Binding<cc::mojom::FrameSinkManagerClient> binding_;

  // A direct connection to FrameSinkManagerImpl. If this is set then
  // |frame_sink_manager_ptr_| must be unbound. For use in browser process only,
  // viz process should not set this.
  FrameSinkManagerImpl* frame_sink_manager_impl_ = nullptr;

  // Per CompositorFrameSink data.
  base::flat_map<FrameSinkId, FrameSinkData> frame_sink_data_map_;

  // Local observers to that receive OnSurfaceCreated() messages from IPC.
  base::ObserverList<FrameSinkObserver> observers_;

  base::WeakPtrFactory<HostFrameSinkManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostFrameSinkManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_FRAME_SINK_MANAGER_H_
