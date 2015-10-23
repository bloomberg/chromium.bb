// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_MUS_APP_H_
#define COMPONENTS_MUS_MUS_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"

namespace mojo {
class ApplicationImpl;
}

namespace ui {
class PlatformEventSource;
}

namespace mus {

class GpuState;
class SurfacesState;

namespace ws {
class ConnectionManager;
}

class MandolineUIServicesApp
    : public mojo::ApplicationDelegate,
      public ws::ConnectionManagerDelegate,
      public mojo::InterfaceFactory<mojom::WindowTreeHostFactory>,
      public mojo::InterfaceFactory<mojom::Gpu>,
      public mojom::WindowTreeHostFactory {
 public:
  MandolineUIServicesApp();
  ~MandolineUIServicesApp() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // ConnectionManagerDelegate:
  void OnNoMoreRootConnections() override;
  ws::ClientConnection* CreateClientConnectionForEmbedAtWindow(
      ws::ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojom::WindowTree> tree_request,
      ConnectionSpecificId creator_id,
      mojo::URLRequestPtr request,
      const ws::WindowId& root_id,
      uint32_t policy_bitmask) override;
  ws::ClientConnection* CreateClientConnectionForEmbedAtWindow(
      ws::ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojom::WindowTree> tree_request,
      ConnectionSpecificId creator_id,
      const ws::WindowId& root_id,
      uint32_t policy_bitmask,
      mojom::WindowTreeClientPtr client) override;

  // mojo::InterfaceFactory<mojom::WindowTreeHostFactory>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojom::WindowTreeHostFactory> request) override;

  // mojo::InterfaceFactory<mojom::Gpu> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojom::Gpu> request) override;

  // mojom::WindowTreeHostFactory implementation.
  void CreateWindowTreeHost(mojo::InterfaceRequest<mojom::WindowTreeHost> host,
                            mojom::WindowTreeHostClientPtr host_client,
                            mojom::WindowTreeClientPtr tree_client) override;

  mojo::WeakBindingSet<mojom::WindowTreeHostFactory> factory_bindings_;
  mojo::ApplicationImpl* app_impl_;
  scoped_ptr<ws::ConnectionManager> connection_manager_;
  scoped_refptr<GpuState> gpu_state_;
  scoped_ptr<ui::PlatformEventSource> event_source_;

  // Surfaces
  scoped_refptr<SurfacesState> surfaces_state_;

  DISALLOW_COPY_AND_ASSIGN(MandolineUIServicesApp);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_MUS_APP_H_
