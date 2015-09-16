// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/view_manager_app.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "components/mus/client_connection.h"
#include "components/mus/connection_manager.h"
#include "components/mus/gles2/gpu_impl.h"
#include "components/mus/public/cpp/args.h"
#include "components/mus/surfaces/surfaces_scheduler.h"
#include "components/mus/view_tree_host_connection.h"
#include "components/mus/view_tree_host_impl.h"
#include "components/mus/view_tree_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/platform_window/x11/x11_window.h"
#endif

using mojo::ApplicationConnection;
using mojo::ApplicationImpl;
using mojo::Gpu;
using mojo::InterfaceRequest;
using mojo::ViewTreeHostFactory;

namespace view_manager {

ViewManagerApp::ViewManagerApp() : app_impl_(nullptr), is_headless_(false) {}

ViewManagerApp::~ViewManagerApp() {
  if (gpu_state_)
    gpu_state_->StopControlThread();
  // Destroy |connection_manager_| first, since it depends on |event_source_|.
  connection_manager_.reset();
}

void ViewManagerApp::Initialize(ApplicationImpl* app) {
  app_impl_ = app;
  tracing_.Initialize(app);
  surfaces_state_ = new surfaces::SurfacesState;

#if !defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  is_headless_ = command_line->HasSwitch(mojo::kUseHeadlessConfig);
  if (!is_headless_) {
#if defined(USE_X11)
    if (command_line->HasSwitch(mojo::kUseX11TestConfig)) {
      XInitThreads();
      ui::test::SetUseOverrideRedirectWindowByDefault(true);
    }
#endif
    gfx::GLSurface::InitializeOneOff();
    event_source_ = ui::PlatformEventSource::CreateDefault();
  }
#endif

  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  connection_manager_.reset(new ConnectionManager(this, surfaces_state_));
}

bool ViewManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  // ViewManager
  connection->AddService<ViewTreeHostFactory>(this);
  // GPU
  connection->AddService<Gpu>(this);
  return true;
}

void ViewManagerApp::OnNoMoreRootConnections() {
  app_impl_->Quit();
}

ClientConnection* ViewManagerApp::CreateClientConnectionForEmbedAtView(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewTree> tree_request,
    mojo::ConnectionSpecificId creator_id,
    mojo::URLRequestPtr request,
    const ViewId& root_id,
    uint32_t policy_bitmask) {
  mojo::ViewTreeClientPtr client;
  app_impl_->ConnectToService(request.Pass(), &client);

  scoped_ptr<ViewTreeImpl> service(new ViewTreeImpl(
      connection_manager, creator_id, root_id, policy_bitmask));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     tree_request.Pass(), client.Pass());
}

ClientConnection* ViewManagerApp::CreateClientConnectionForEmbedAtView(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojo::ViewTree> tree_request,
    mojo::ConnectionSpecificId creator_id,
    const ViewId& root_id,
    uint32_t policy_bitmask,
    mojo::ViewTreeClientPtr client) {
  scoped_ptr<ViewTreeImpl> service(new ViewTreeImpl(
      connection_manager, creator_id, root_id, policy_bitmask));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     tree_request.Pass(), client.Pass());
}

void ViewManagerApp::Create(ApplicationConnection* connection,
                            InterfaceRequest<ViewTreeHostFactory> request) {
  factory_bindings_.AddBinding(this, request.Pass());
}

void ViewManagerApp::Create(mojo::ApplicationConnection* connection,
                            mojo::InterfaceRequest<Gpu> request) {
  if (!gpu_state_.get())
    gpu_state_ = new gles2::GpuState;
  new gles2::GpuImpl(request.Pass(), gpu_state_);
}

void ViewManagerApp::CreateViewTreeHost(
    mojo::InterfaceRequest<mojo::ViewTreeHost> host,
    mojo::ViewTreeHostClientPtr host_client,
    mojo::ViewTreeClientPtr tree_client) {
  DCHECK(connection_manager_.get());

  // TODO(fsamuel): We need to make sure that only the window manager can create
  // new roots.
  ViewTreeHostImpl* host_impl = new ViewTreeHostImpl(
      host_client.Pass(), connection_manager_.get(), is_headless_, app_impl_,
      gpu_state_, surfaces_state_);

  // ViewTreeHostConnection manages its own lifetime.
  host_impl->Init(new ViewTreeHostConnectionImpl(
      host.Pass(), make_scoped_ptr(host_impl), tree_client.Pass(),
      connection_manager_.get()));
}

}  // namespace view_manager
