// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/mus_app.h"

#include "base/stl_util.h"
#include "components/mus/gles2/gpu_impl.h"
#include "components/mus/public/cpp/args.h"
#include "components/mus/surfaces/surfaces_scheduler.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "base/command_line.h"
#include "ui/platform_window/x11/x11_window.h"
#endif

using mojo::ApplicationConnection;
using mojo::ApplicationImpl;
using mojo::InterfaceRequest;
using mus::mojom::WindowTreeHostFactory;
using mus::mojom::Gpu;

namespace mus {

MandolineUIServicesApp::MandolineUIServicesApp()
    : app_impl_(nullptr) {}

MandolineUIServicesApp::~MandolineUIServicesApp() {
  if (gpu_state_)
    gpu_state_->StopControlThread();
  // Destroy |connection_manager_| first, since it depends on |event_source_|.
  connection_manager_.reset();
}

void MandolineUIServicesApp::Initialize(ApplicationImpl* app) {
  app_impl_ = app;
  tracing_.Initialize(app);
  surfaces_state_ = new SurfacesState;

#if defined(USE_X11)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUseX11TestConfig)) {
    XInitThreads();
    ui::test::SetUseOverrideRedirectWindowByDefault(true);
  }
#endif

#if !defined(OS_ANDROID)
  gfx::GLSurface::InitializeOneOff();
  event_source_ = ui::PlatformEventSource::CreateDefault();
#endif

  if (!gpu_state_.get())
    gpu_state_ = new GpuState;
  connection_manager_.reset(new ConnectionManager(this, surfaces_state_));
}

bool MandolineUIServicesApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<WindowTreeHostFactory>(this);
  connection->AddService<Gpu>(this);
  return true;
}

void MandolineUIServicesApp::OnNoMoreRootConnections() {
  app_impl_->Quit();
}

ClientConnection*
MandolineUIServicesApp::CreateClientConnectionForEmbedAtWindow(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojom::WindowTree> tree_request,
    ConnectionSpecificId creator_id,
    mojo::URLRequestPtr request,
    const WindowId& root_id,
    uint32_t policy_bitmask) {
  mojom::WindowTreeClientPtr client;
  app_impl_->ConnectToService(request.Pass(), &client);

  scoped_ptr<WindowTreeImpl> service(new WindowTreeImpl(
      connection_manager, creator_id, root_id, policy_bitmask));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     tree_request.Pass(), client.Pass());
}

ClientConnection*
MandolineUIServicesApp::CreateClientConnectionForEmbedAtWindow(
    ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojom::WindowTree> tree_request,
    ConnectionSpecificId creator_id,
    const WindowId& root_id,
    uint32_t policy_bitmask,
    mojom::WindowTreeClientPtr client) {
  scoped_ptr<WindowTreeImpl> service(new WindowTreeImpl(
      connection_manager, creator_id, root_id, policy_bitmask));
  return new DefaultClientConnection(service.Pass(), connection_manager,
                                     tree_request.Pass(), client.Pass());
}

void MandolineUIServicesApp::Create(
    ApplicationConnection* connection,
    InterfaceRequest<WindowTreeHostFactory> request) {
  factory_bindings_.AddBinding(this, request.Pass());
}

void MandolineUIServicesApp::Create(mojo::ApplicationConnection* connection,
                                    mojo::InterfaceRequest<Gpu> request) {
  if (!gpu_state_.get())
    gpu_state_ = new GpuState;
  new GpuImpl(request.Pass(), gpu_state_);
}

void MandolineUIServicesApp::CreateWindowTreeHost(
    mojo::InterfaceRequest<mojom::WindowTreeHost> host,
    mojom::WindowTreeHostClientPtr host_client,
    mojom::WindowTreeClientPtr tree_client) {
  DCHECK(connection_manager_.get());

  // TODO(fsamuel): We need to make sure that only the window manager can create
  // new roots.
  WindowTreeHostImpl* host_impl =
      new WindowTreeHostImpl(host_client.Pass(), connection_manager_.get(),
                             app_impl_, gpu_state_, surfaces_state_);

  // WindowTreeHostConnection manages its own lifetime.
  host_impl->Init(new WindowTreeHostConnectionImpl(
      host.Pass(), make_scoped_ptr(host_impl), tree_client.Pass(),
      connection_manager_.get()));
}

}  // namespace mus
