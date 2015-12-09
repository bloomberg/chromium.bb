// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/mus_app.h"

#include "base/stl_util.h"
#include "components/mus/common/args.h"
#include "components/mus/gles2/gpu_impl.h"
#include "components/mus/surfaces/surfaces_scheduler.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/forwarding_window_manager.h"
#include "components/mus/ws/window_tree_host_connection.h"
#include "components/mus/ws/window_tree_host_impl.h"
#include "components/mus/ws/window_tree_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
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
  surfaces_state_ = new SurfacesState;

#if defined(USE_X11)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUseX11TestConfig)) {
    XInitThreads();
    ui::test::SetUseOverrideRedirectWindowByDefault(true);
  }
#endif

  bool hardware_rendering_available = true;
#if !defined(OS_ANDROID)
  hardware_rendering_available = gfx::GLSurface::InitializeOneOff();
  event_source_ = ui::PlatformEventSource::CreateDefault();
#endif

  // TODO(rjkroege): It is possible that we might want to generalize the
  // GpuState object.
  if (!gpu_state_.get())
    gpu_state_ = new GpuState(hardware_rendering_available);
  connection_manager_.reset(new ws::ConnectionManager(this, surfaces_state_));

  tracing_.Initialize(app);
}

bool MandolineUIServicesApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<Gpu>(this);
  connection->AddService<mojom::WindowManager>(this);
  connection->AddService<WindowTreeHostFactory>(this);
  return true;
}

void MandolineUIServicesApp::OnFirstRootConnectionCreated() {
  WindowManagerRequests requests;
  requests.swap(pending_window_manager_requests_);
  for (auto& request : requests)
    Create(nullptr, std::move(*request));
}

void MandolineUIServicesApp::OnNoMoreRootConnections() {
  app_impl_->Quit();
}

ws::ClientConnection*
MandolineUIServicesApp::CreateClientConnectionForEmbedAtWindow(
    ws::ConnectionManager* connection_manager,
    mojo::InterfaceRequest<mojom::WindowTree> tree_request,
    ConnectionSpecificId creator_id,
    const ws::WindowId& root_id,
    uint32_t policy_bitmask,
    mojom::WindowTreeClientPtr client) {
  scoped_ptr<ws::WindowTreeImpl> service(new ws::WindowTreeImpl(
      connection_manager, creator_id, root_id, policy_bitmask));
  return new ws::DefaultClientConnection(std::move(service), connection_manager,
                                         std::move(tree_request),
                                         std::move(client));
}

void MandolineUIServicesApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojom::WindowManager> request) {
  if (!connection_manager_->has_tree_host_connections()) {
    pending_window_manager_requests_.push_back(make_scoped_ptr(
        new mojo::InterfaceRequest<mojom::WindowManager>(std::move(request))));
    return;
  }
  if (!window_manager_impl_) {
    window_manager_impl_.reset(
        new ws::ForwardingWindowManager(connection_manager_.get()));
  }
  window_manager_bindings_.AddBinding(window_manager_impl_.get(),
                                      std::move(request));
}

void MandolineUIServicesApp::Create(
    ApplicationConnection* connection,
    InterfaceRequest<WindowTreeHostFactory> request) {
  factory_bindings_.AddBinding(this, std::move(request));
}

void MandolineUIServicesApp::Create(mojo::ApplicationConnection* connection,
                                    mojo::InterfaceRequest<Gpu> request) {
  DCHECK(gpu_state_);
  new GpuImpl(std::move(request), gpu_state_);
}

void MandolineUIServicesApp::CreateWindowTreeHost(
    mojo::InterfaceRequest<mojom::WindowTreeHost> host,
    mojom::WindowTreeHostClientPtr host_client,
    mojom::WindowTreeClientPtr tree_client,
    mojom::WindowManagerPtr window_manager) {
  DCHECK(connection_manager_);

  // TODO(fsamuel): We need to make sure that only the window manager can create
  // new roots.
  ws::WindowTreeHostImpl* host_impl = new ws::WindowTreeHostImpl(
      std::move(host_client), connection_manager_.get(), app_impl_, gpu_state_,
      surfaces_state_, std::move(window_manager));

  // WindowTreeHostConnection manages its own lifetime.
  host_impl->Init(new ws::WindowTreeHostConnectionImpl(
      std::move(host), make_scoped_ptr(host_impl), std::move(tree_client),
      connection_manager_.get()));
}

}  // namespace mus
