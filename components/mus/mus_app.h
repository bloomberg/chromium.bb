// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_MUS_APP_H_
#define COMPONENTS_MUS_MUS_APP_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/display.mojom.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/user_access_manager.mojom.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/mus/ws/platform_display_init_params.h"
#include "components/mus/ws/user_id.h"
#include "components/mus/ws/window_server_delegate.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
class Connector;
}

namespace ui {
class PlatformEventSource;
}

namespace mus {
namespace ws {
class ForwardingWindowManager;
class WindowServer;
}

class MandolineUIServicesApp
    : public mojo::ShellClient,
      public ws::WindowServerDelegate,
      public mojo::InterfaceFactory<mojom::DisplayManager>,
      public mojo::InterfaceFactory<mojom::UserAccessManager>,
      public mojo::InterfaceFactory<mojom::WindowManagerFactoryService>,
      public mojo::InterfaceFactory<mojom::WindowTreeFactory>,
      public mojo::InterfaceFactory<mojom::WindowTreeHostFactory>,
      public mojo::InterfaceFactory<mojom::Gpu> {
 public:
  MandolineUIServicesApp();
  ~MandolineUIServicesApp() override;

 private:
  // Holds InterfaceRequests received before the first WindowTreeHost Display
  // has been established.
  struct PendingRequest;
  struct UserState;

  using UserIdToUserState = std::map<ws::UserId, scoped_ptr<UserState>>;

  void InitializeResources(mojo::Connector* connector);

  // Returns the user specific state for the user id of |connection|. MusApp
  // owns the return value.
  // TODO(sky): if we allow removal of user ids then we need to close anything
  // associated with the user (all incoming pipes...) on removal.
  UserState* GetUserState(mojo::Connection* connection);

  void AddUserIfNecessary(mojo::Connection* connection);

  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;
  void ShellConnectionLost() override;

  // WindowServerDelegate:
  void OnFirstDisplayReady() override;
  void OnNoMoreDisplays() override;
  scoped_ptr<ws::WindowTreeBinding> CreateWindowTreeBindingForEmbedAtWindow(
      ws::WindowServer* window_server,
      ws::WindowTree* tree,
      mojom::WindowTreeRequest tree_request,
      mojom::WindowTreeClientPtr client) override;
  void CreateDefaultDisplays() override;

  // mojo::InterfaceFactory<mojom::DisplayManager> implementation.
  void Create(mojo::Connection* connection,
              mojom::DisplayManagerRequest request) override;

  // mojo::InterfaceFactory<mojom::UserAccessManager> implementation.
  void Create(mojo::Connection* connection,
              mojom::UserAccessManagerRequest request) override;

  // mojo::InterfaceFactory<mojom::WindowManagerFactoryService> implementation.
  void Create(mojo::Connection* connection,
              mojom::WindowManagerFactoryServiceRequest request) override;

  // mojo::InterfaceFactory<mojom::WindowTreeFactory>:
  void Create(mojo::Connection* connection,
              mojom::WindowTreeFactoryRequest request) override;

  // mojo::InterfaceFactory<mojom::WindowTreeHostFactory>:
  void Create(mojo::Connection* connection,
              mojom::WindowTreeHostFactoryRequest request) override;

  // mojo::InterfaceFactory<mojom::Gpu> implementation.
  void Create(mojo::Connection* connection, mojom::GpuRequest request) override;

  ws::PlatformDisplayInitParams platform_display_init_params_;
  scoped_ptr<ws::WindowServer> window_server_;
  scoped_ptr<ui::PlatformEventSource> event_source_;
  mojo::TracingImpl tracing_;
  using PendingRequests = std::vector<scoped_ptr<PendingRequest>>;
  PendingRequests pending_requests_;

  UserIdToUserState user_id_to_user_state_;

  DISALLOW_COPY_AND_ASSIGN(MandolineUIServicesApp);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_MUS_APP_H_
