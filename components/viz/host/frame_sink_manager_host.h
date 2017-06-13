// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_FRAME_SINK_MANAGER_HOST_H_
#define COMPONENTS_VIZ_HOST_FRAME_SINK_MANAGER_HOST_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
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
class MojoFrameSinkManager;

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

  // See frame_sink_manager.mojom for descriptions.
  void CreateCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);
  void RegisterFrameSinkHierarchy(const cc::FrameSinkId& parent_frame_sink_id,
                                  const cc::FrameSinkId& child_frame_sink_id);
  void UnregisterFrameSinkHierarchy(const cc::FrameSinkId& parent_frame_sink_id,
                                    const cc::FrameSinkId& child_frame_sink_id);

  static void ConnectWithInProcessFrameSinkManager(
      FrameSinkManagerHost* host,
      MojoFrameSinkManager* manager);

 private:
  // cc::mojom::FrameSinkManagerClient:
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;

  // Mojo connection to the FrameSinkManager.
  cc::mojom::FrameSinkManagerPtr frame_sink_manager_ptr_;

  // Mojo connection back from the FrameSinkManager.
  mojo::Binding<cc::mojom::FrameSinkManagerClient> binding_;

  // Local observers to that receive OnSurfaceCreated() messages from IPC.
  base::ObserverList<FrameSinkObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerHost);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_FRAME_SINK_MANAGER_HOST_H_
