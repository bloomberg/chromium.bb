// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_APP_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_APP_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/surface_manager.h"
#include "components/view_manager/connection_manager_delegate.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "components/view_manager/surfaces/surfaces_delegate.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/tracing_impl.h"

namespace gles2 {
class GpuState;
}

namespace mojo {
class ApplicationImpl;
}

namespace surfaces {
class DisplayImpl;
class SurfacesImpl;
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
                       public mojo::InterfaceFactory<mojo::Surface>,
                       public mojo::InterfaceFactory<mojo::ViewManagerRoot>,
                       public mojo::InterfaceFactory<mojo::Gpu>,
                       public surfaces::SurfacesDelegate {
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
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      mojo::URLRequestPtr request,
      const ViewId& root_id) override;
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      const ViewId& root_id,
      mojo::ViewManagerClientPtr view_manager_client) override;

  // mojo::InterfaceFactory<mojo::ViewManagerRoot>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ViewManagerRoot> request) override;

  // mojo::InterfaceFactory<mojo::Gpu> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::Gpu> request) override;

  // InterfaceFactory<Surface> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::Surface> request) override;

  // SurfacesDelegat implementation.
  void OnSurfaceConnectionClosed(surfaces::SurfacesImpl* surface) override;

  // ViewManager
  mojo::ApplicationImpl* app_impl_;
  scoped_ptr<ConnectionManager> connection_manager_;
  mojo::TracingImpl tracing_;
  scoped_refptr<gles2::GpuState> gpu_state_;
  scoped_ptr<ui::PlatformEventSource> event_source_;
  bool is_headless_;

  // Surfaces
  std::set<surfaces::SurfacesImpl*> surfaces_;
  scoped_refptr<surfaces::SurfacesState> surfaces_state_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_APP_H_
