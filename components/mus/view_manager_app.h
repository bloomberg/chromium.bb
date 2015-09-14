// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_VIEW_MANAGER_APP_H_
#define COMPONENTS_MUS_VIEW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/surface_manager.h"
#include "components/mus/connection_manager_delegate.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/tracing_impl.h"
#include "mojo/common/weak_binding_set.h"

namespace gles2 {
class GpuState;
}

namespace mojo {
class ApplicationImpl;
}

namespace surfaces {
class SurfacesScheduler;
class SurfacesState;
}

namespace ui {
class PlatformEventSource;
}

namespace view_manager {

class ConnectionManager;

class ViewManagerApp : public mojo::ApplicationDelegate,
                       public ConnectionManagerDelegate,
                       public mojo::InterfaceFactory<mojo::ViewTreeHostFactory>,
                       public mojo::InterfaceFactory<mojo::Gpu>,
                       public mojo::ViewTreeHostFactory {
 public:
  ViewManagerApp();
  ~ViewManagerApp() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // ConnectionManagerDelegate:
  void OnNoMoreRootConnections() override;
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewTree> tree_request,
      mojo::ConnectionSpecificId creator_id,
      mojo::URLRequestPtr request,
      const ViewId& root_id) override;
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewTree> tree_request,
      mojo::ConnectionSpecificId creator_id,
      const ViewId& root_id,
      mojo::ViewTreeClientPtr client) override;

  // mojo::InterfaceFactory<mojo::ViewTreeHostFactory>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::ViewTreeHostFactory> request) override;

  // mojo::InterfaceFactory<mojo::Gpu> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::Gpu> request) override;

  // mojo::ViewTreeHostFactory implementation.
  void CreateViewTreeHost(mojo::InterfaceRequest<mojo::ViewTreeHost> host,
                          mojo::ViewTreeHostClientPtr host_client,
                          mojo::ViewTreeClientPtr tree_client) override;

  mojo::WeakBindingSet<mojo::ViewTreeHostFactory> factory_bindings_;
  mojo::ApplicationImpl* app_impl_;
  scoped_ptr<ConnectionManager> connection_manager_;
  mojo::TracingImpl tracing_;
  scoped_refptr<gles2::GpuState> gpu_state_;
  scoped_ptr<ui::PlatformEventSource> event_source_;
  bool is_headless_;

  // Surfaces
  scoped_refptr<surfaces::SurfacesState> surfaces_state_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}  // namespace view_manager

#endif  // COMPONENTS_MUS_VIEW_MANAGER_APP_H_
