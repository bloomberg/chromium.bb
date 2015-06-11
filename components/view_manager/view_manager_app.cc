// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_app.h"

#include "base/command_line.h"
#include "components/view_manager/client_connection.h"
#include "components/view_manager/connection_manager.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/public/cpp/args.h"
#include "components/view_manager/view_manager_service_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"

using mojo::ApplicationConnection;
using mojo::ApplicationImpl;
using mojo::Gpu;
using mojo::InterfaceRequest;
using mojo::ViewManagerRoot;
using mojo::ViewManagerService;

namespace view_manager {

ViewManagerApp::ViewManagerApp() : app_impl_(nullptr), is_headless_(false) {
}

ViewManagerApp::~ViewManagerApp() {}

void ViewManagerApp::Initialize(ApplicationImpl* app) {
  app_impl_ = app;
  tracing_.Initialize(app);

#if !defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  is_headless_ = command_line->HasSwitch(mojo::kUseHeadlessConfig);
  if (!is_headless_) {
    event_source_ = ui::PlatformEventSource::CreateDefault();
    if (command_line->HasSwitch(mojo::kUseTestConfig))
      gfx::GLSurface::InitializeOneOffForTests();
    else
      gfx::GLSurface::InitializeOneOff();
  }
#endif

  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  scoped_ptr<DefaultDisplayManager> display_manager(new DefaultDisplayManager(
      is_headless_,
      app_impl_,
      gpu_state_,
      base::Bind(&ViewManagerApp::OnLostConnectionToWindowManager,
                 base::Unretained(this))));
  connection_manager_.reset(
      new ConnectionManager(this, display_manager.Pass()));
}

bool ViewManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {

  // |connection| originates from the WindowManager. Let it connect directly
  // to the ViewManager.
  connection->AddService<ViewManagerService>(this);
  connection->AddService<ViewManagerRoot>(this);
  connection->AddService<Gpu>(this);

  return true;
}

void ViewManagerApp::OnLostConnectionToWindowManager() {
  app_impl_->Terminate();
}

ClientConnection* ViewManagerApp::CreateClientConnectionForEmbedAtView(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
    mojo::ConnectionSpecificId creator_id,
    mojo::URLRequestPtr request,
    const ViewId& root_id) {
  mojo::ViewManagerClientPtr client;
  app_impl_->ConnectToService(request.Pass(), &client);

  scoped_ptr<ViewManagerServiceImpl> service(
      new ViewManagerServiceImpl(connection_manager, creator_id, root_id));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     service_request.Pass(), client.Pass());
}

ClientConnection* ViewManagerApp::CreateClientConnectionForEmbedAtView(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewManagerService> service_request,
    mojo::ConnectionSpecificId creator_id,
    const ViewId& root_id,
    mojo::ViewManagerClientPtr view_manager_client) {
  scoped_ptr<ViewManagerServiceImpl> service(
      new ViewManagerServiceImpl(connection_manager, creator_id, root_id));
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
      connection_manager_.get(), kInvalidConnectionId, RootViewId(0)));
  mojo::ViewManagerClientPtr client;
  connection->ConnectToService(&client);
  scoped_ptr<ClientConnection> client_connection(
      new DefaultClientConnection(service.Pass(), connection_manager_.get(),
                                  request.Pass(), client.Pass()));
  connection_manager_->SetWindowManagerClientConnection(
      client_connection.Pass());
}

void ViewManagerApp::Create(ApplicationConnection* connection,
                            InterfaceRequest<ViewManagerRoot> request) {
  if (view_manager_root_binding_.get()) {
    VLOG(1) << "ViewManagerRoot requested more than once.";
    return;
  }

  // ConfigureIncomingConnection() must have been called before getting here.
  DCHECK(connection_manager_.get());
  view_manager_root_binding_.reset(new mojo::Binding<ViewManagerRoot>(
      connection_manager_.get(), request.Pass()));
  view_manager_root_binding_->set_error_handler(this);
}

void ViewManagerApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<Gpu> request) {
  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  new gles2::GpuImpl(request.Pass(), gpu_state_);
}

void ViewManagerApp::OnConnectionError() {
  app_impl_->Terminate();
}

}  // namespace view_manager
