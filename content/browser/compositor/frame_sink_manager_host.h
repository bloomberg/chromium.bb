// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_FRAME_SINK_MANAGER_HOST_H_
#define CONTENT_BROWSER_COMPOSITOR_FRAME_SINK_MANAGER_HOST_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "components/viz/frame_sinks/mojo_frame_sink_manager.h"
#include "content/browser/compositor/frame_sink_observer.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace cc {
class SurfaceInfo;
class SurfaceManager;
}

namespace content {

// Browser side implementation of mojom::FrameSinkManager. Manages frame sinks
// and is intended to replace SurfaceManager.
class CONTENT_EXPORT FrameSinkManagerHost
    : NON_EXPORTED_BASE(cc::mojom::FrameSinkManagerClient) {
 public:
  FrameSinkManagerHost();
  ~FrameSinkManagerHost() override;

  cc::SurfaceManager* surface_manager();

  // Start Mojo connection to FrameSinkManager. Most tests won't need this.
  void ConnectToFrameSinkManager();

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

 private:
  // cc::mojom::FrameSinkManagerClient:
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info) override;

  // Mojo connection to |frame_sink_manager_|.
  cc::mojom::FrameSinkManagerPtr frame_sink_manager_ptr_;

  // Mojo connection back from |frame_sink_manager_|.
  mojo::Binding<cc::mojom::FrameSinkManagerClient> binding_;

  // This is owned here so that SurfaceManager will be accessible in process.
  // Other than using SurfaceManager, access to |frame_sink_manager_| should
  // happen using Mojo. See http://crbug.com/657959.
  viz::MojoFrameSinkManager frame_sink_manager_;

  // Local observers to that receive OnSurfaceCreated() messages from IPC.
  base::ObserverList<FrameSinkObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_FRAME_SINK_MANAGER_HOST_H_
