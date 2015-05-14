// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_app.h"

#include "components/view_manager/client_connection.h"
#include "components/view_manager/connection_manager.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/view_manager_service_impl.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

using mojo::ApplicationConnection;
using mojo::ApplicationImpl;
using mojo::InterfaceRequest;
using mojo::ViewManagerService;
using mojo::WindowManagerInternalClient;

namespace view_manager {

ViewManagerApp::ViewManagerApp()
    : app_impl_(nullptr), wm_app_connection_(nullptr) {
}

ViewManagerApp::~ViewManagerApp() {}

void ViewManagerApp::Initialize(ApplicationImpl* app) {
  app_impl_ = app;
  tracing_.Initialize(app);
}

bool ViewManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  if (connection_manager_.get()) {
    VLOG(1) << "ViewManager allows only one window manager connection.";
    return false;
  }
  wm_app_connection_ = connection;
  // |connection| originates from the WindowManager. Let it connect directly
  // to the ViewManager and WindowManagerInternalClient.
  connection->AddService<ViewManagerService>(this);
  connection->AddService<WindowManagerInternalClient>(this);
  connection->ConnectToService(&wm_internal_);
  // If no ServiceProvider has been sent, refuse the connection.
  if (!wm_internal_)
    return false;
  wm_internal_.set_error_handler(this);

  scoped_ptr<DefaultDisplayManager> display_manager(new DefaultDisplayManager(
      app_impl_, base::Bind(&ViewManagerApp::OnLostConnectionToWindowManager,
                            base::Unretained(this))));
  connection_manager_.reset(
      new ConnectionManager(this, display_manager.Pass(), wm_internal_.get()));
  return true;
}

void ViewManagerApp::OnLostConnectionToWindowManager() {
  ApplicationImpl::Terminate();
}

ClientConnection* ViewManagerApp::CreateClientConnectionForEmbedAtView(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
    mojo::ConnectionSpecificId creator_id,
    const std::string& creator_url,
    const std::string& url,
    const ViewId& root_id) {
  mojo::ViewManagerClientPtr client;
  app_impl_->ConnectToService(url, &client);

  scoped_ptr<ViewManagerServiceImpl> service(new ViewManagerServiceImpl(
      connection_manager, creator_id, creator_url, url, root_id));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     service_request.Pass(), client.Pass());
}

ClientConnection* ViewManagerApp::CreateClientConnectionForEmbedAtView(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
    mojo::ConnectionSpecificId creator_id,
    const std::string& creator_url,
    const ViewId& root_id,
    mojo::ViewManagerClientPtr view_manager_client) {
  scoped_ptr<ViewManagerServiceImpl> service(new ViewManagerServiceImpl(
      connection_manager, creator_id, creator_url, std::string(), root_id));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     service_request.Pass(),
                                     view_manager_client.Pass());
}

void ViewManagerApp::Create(ApplicationConnection* connection,
                            InterfaceRequest<ViewManagerService> request) {
  if (connection_manager_->has_window_manager_client_connection()) {
    VLOG(1) << "ViewManager interface requested more than once.";
    return;
  }

  scoped_ptr<ViewManagerServiceImpl> service(new ViewManagerServiceImpl(
      connection_manager_.get(), kInvalidConnectionId, std::string(),
      std::string("mojo:window_manager"), RootViewId()));
  mojo::ViewManagerClientPtr client;
  wm_internal_client_request_ = GetProxy(&client);
  scoped_ptr<ClientConnection> client_connection(
      new DefaultClientConnection(service.Pass(), connection_manager_.get(),
                                  request.Pass(), client.Pass()));
  connection_manager_->SetWindowManagerClientConnection(
      client_connection.Pass());
}

void ViewManagerApp::Create(
    ApplicationConnection* connection,
    InterfaceRequest<WindowManagerInternalClient> request) {
  if (wm_internal_client_binding_.get()) {
    VLOG(1) << "WindowManagerInternalClient requested more than once.";
    return;
  }

  // ConfigureIncomingConnection() must have been called before getting here.
  DCHECK(connection_manager_.get());
  wm_internal_client_binding_.reset(
      new mojo::Binding<WindowManagerInternalClient>(connection_manager_.get(),
                                                     request.Pass()));
  wm_internal_client_binding_->set_error_handler(this);
  wm_internal_->SetViewManagerClient(
      wm_internal_client_request_.PassMessagePipe());
}

void ViewManagerApp::OnConnectionError() {
  ApplicationImpl::Terminate();
}

}  // namespace view_manager
