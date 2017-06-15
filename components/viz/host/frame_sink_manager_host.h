// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_FRAME_SINK_MANAGER_HOST_H_
#define COMPONENTS_VIZ_HOST_FRAME_SINK_MANAGER_HOST_H_

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "components/viz/host/frame_sink_observer.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace cc {
class SurfaceInfo;
class SurfaceManager;
}

namespace viz {

// Browser side implementation of mojom::FrameSinkManager, to be used from the
// UI thread. Manages frame sinks and is intended to replace SurfaceManager.
class VIZ_HOST_EXPORT FrameSinkManagerHost
    : NON_EXPORTED_BASE(cc::mojom::FrameSinkManagerClient) {
 public:
  FrameSinkManagerHost();
  ~FrameSinkManagerHost() override;

  // Binds |this| as a FrameSinkManagerClient to the |request|. May only be
  // called once.
  void BindManagerClientAndSetManagerPtr(
      cc::mojom::FrameSinkManagerClientRequest request,
      cc::mojom::FrameSinkManagerPtr ptr);

  void AddObserver(FrameSinkObserver* observer);
  void RemoveObserver(FrameSinkObserver* observer);

  // Creates a connection between client to viz, using |request| and |client|,
  // that allows the client to submit CompositorFrames. When no longer needed,
  // call DestroyCompositorFrameSink().
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  // Destroys a client connection. Will call UnregisterFrameSinkHierarchy() with
  // the registered parent if there is one.
  void DestroyCompositorFrameSink(const cc::FrameSinkId& frame_sink_id);

  // Registers FrameSink hierarchy. Clients can call this multiple times to
  // reparent without calling UnregisterFrameSinkHierarchy(). If a client uses
  // MojoCompositorFrameSink, then CreateCompositorFrameSink() should be called
  // before this.
  void RegisterFrameSinkHierarchy(const cc::FrameSinkId& parent_frame_sink_id,
                                  const cc::FrameSinkId& child_frame_sink_id);

  // Unregisters FrameSink hierarchy. Client must have registered FrameSink
  // hierarchy before unregistering.
  void UnregisterFrameSinkHierarchy(const cc::FrameSinkId& parent_frame_sink_id,
                                    const cc::FrameSinkId& child_frame_sink_id);

 private:
  struct FrameSinkData {
    FrameSinkData();
    FrameSinkData(FrameSinkData&& other);
    ~FrameSinkData();
    FrameSinkData& operator=(FrameSinkData&& other);

    // The FrameSinkId registered as the parent in the BeginFrame hierarchy.
    // This mirrors state in viz.
    base::Optional<cc::FrameSinkId> parent;

    // The private interface that gives the host control over the
    // CompositorFrameSink connection between the client and viz.
    cc::mojom::MojoCompositorFrameSinkPrivatePtr private_interface;

    DISALLOW_COPY_AND_ASSIGN(FrameSinkData);
  };

  // cc::mojom::FrameSinkManagerClient:
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;

  // Mojo connection to the FrameSinkManager.
  cc::mojom::FrameSinkManagerPtr frame_sink_manager_ptr_;

  // Mojo connection back from the FrameSinkManager.
  mojo::Binding<cc::mojom::FrameSinkManagerClient> binding_;

  // Per CompositorFrameSink data.
  base::flat_map<cc::FrameSinkId, FrameSinkData> frame_sink_data_map_;

  // Local observers to that receive OnSurfaceCreated() messages from IPC.
  base::ObserverList<FrameSinkObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerHost);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_FRAME_SINK_MANAGER_HOST_H_
