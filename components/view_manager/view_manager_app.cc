// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_app.h"

#include "base/command_line.h"
#include "components/view_manager/client_connection.h"
#include "components/view_manager/connection_manager.h"
#include "components/view_manager/public/cpp/args.h"
#include "components/view_manager/view_manager_root_connection.h"
#include "components/view_manager/view_manager_root_impl.h"
#include "components/view_manager/view_manager_service_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_surface_test_support.h"

using mojo::ApplicationConnection;
using mojo::ApplicationImpl;
using mojo::Gpu;
using mojo::InterfaceRequest;
using mojo::ViewManagerRoot;
using mojo::ViewManagerService;

namespace view_manager {

ViewManagerApp::ViewManagerApp() : app_impl_(nullptr), is_headless_(false) {
}

ViewManagerApp::~ViewManagerApp() {
  if (gpu_state_)
    gpu_state_->StopControlThread();
}

void ViewManagerApp::Initialize(ApplicationImpl* app) {
  app_impl_ = app;
  tracing_.Initialize(app);

#if !defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  is_headless_ = command_line->HasSwitch(mojo::kUseHeadlessConfig);
  if (!is_headless_) {
    event_source_ = ui::PlatformEventSource::CreateDefault();
    if (command_line->HasSwitch(mojo::kUseTestConfig))
      gfx::GLSurfaceTestSupport::InitializeOneOff();
    else
      gfx::GLSurface::InitializeOneOff();
  }
#endif

  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  connection_manager_.reset(new ConnectionManager(this));
}

bool ViewManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<ViewManagerRoot>(this);
  connection->AddService<Gpu>(this);

  return true;
}

void ViewManagerApp::OnNoMoreRootConnections() {
  app_impl_->Quit();
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
                            InterfaceRequest<ViewManagerRoot> request) {
  DCHECK(connection_manager_.get());
  // TODO(fsamuel): We need to make sure that only the window manager can create
  // new roots.
  ViewManagerRootImpl* view_manager_root = new ViewManagerRootImpl(
      connection_manager_.get(), is_headless_, app_impl_, gpu_state_);

  mojo::ViewManagerClientPtr client;
  connection->ConnectToService(&client);

  // ViewManagerRootConnection manages its own lifetime.
  view_manager_root->Init(new ViewManagerRootConnectionImpl(
      request.Pass(), make_scoped_ptr(view_manager_root), client.Pass(),
      connection_manager_.get()));
}

void ViewManagerApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<Gpu> request) {
  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  new gles2::GpuImpl(request.Pass(), gpu_state_);
}

}  // namespace view_manager
