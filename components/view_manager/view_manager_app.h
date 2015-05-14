// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_APP_H_
#define COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_APP_H_

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/connection_manager_delegate.h"
#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/tracing_impl.h"

namespace mojo {
class ApplicationImpl;
}

namespace view_manager {

class ConnectionManager;

class ViewManagerApp : public mojo::ApplicationDelegate,
                       public ConnectionManagerDelegate,
                       public mojo::ErrorHandler,
                       public mojo::InterfaceFactory<mojo::ViewManagerRoot>,
                       public mojo::InterfaceFactory<mojo::ViewManagerService> {
 public:
  ViewManagerApp();
  ~ViewManagerApp() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // ConnectionManagerDelegate:
  void OnLostConnectionToWindowManager() override;
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      const std::string& creator_url,
      const std::string& url,
      const ViewId& root_id) override;
  ClientConnection* CreateClientConnectionForEmbedAtView(
      ConnectionManager* connection_manager,
      mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
      mojo::ConnectionSpecificId creator_id,
      const std::string& creator_url,
      const ViewId& root_id,
      mojo::ViewManagerClientPtr view_manager_client) override;

  // mojo::InterfaceFactory<mojo::ViewManagerService>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::ViewManagerService> request) override;

  // mojo::InterfaceFactory<mojo::ViewManagerRoot>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ViewManagerRoot> request) override;

  // ErrorHandler (for |wm_internal_| and |wm_internal_client_binding_|).
  void OnConnectionError() override;

  mojo::ApplicationImpl* app_impl_;
  mojo::ApplicationConnection* wm_app_connection_;
  scoped_ptr<mojo::Binding<mojo::ViewManagerRoot>> view_manager_root_binding_;
  scoped_ptr<ConnectionManager> connection_manager_;
  mojo::TracingImpl tracing_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_VIEW_MANAGER_APP_H_
