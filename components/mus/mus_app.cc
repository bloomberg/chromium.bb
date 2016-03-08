// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/mus_app.h"

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/mus/common/args.h"
#include "components/mus/gles2/gpu_impl.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/user_display_manager.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "components/mus/ws/window_tree_factory.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "base/command_line.h"
#include "ui/platform_window/x11/x11_window.h"
#elif defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

using mojo::Connection;
using mojo::InterfaceRequest;
using mus::mojom::WindowTreeHostFactory;
using mus::mojom::Gpu;

namespace mus {

namespace {

const char kResourceFileStrings[] = "mus_app_resources_strings.pak";
const char kResourceFile100[] = "mus_app_resources_100.pak";
const char kResourceFile200[] = "mus_app_resources_200.pak";

}  // namespace

// TODO(sky): this is a pretty typical pattern, make it easier to do.
struct MandolineUIServicesApp::PendingRequest {
  scoped_ptr<mojo::InterfaceRequest<mojom::DisplayManager>> dm_request;
  scoped_ptr<mojo::InterfaceRequest<mojom::WindowTreeFactory>> wtf_request;
};

MandolineUIServicesApp::MandolineUIServicesApp()
    : connector_(nullptr) {}

MandolineUIServicesApp::~MandolineUIServicesApp() {
  if (gpu_state_)
    gpu_state_->StopThreads();
  // Destroy |connection_manager_| first, since it depends on |event_source_|.
  connection_manager_.reset();
}

void MandolineUIServicesApp::InitializeResources(mojo::Connector* connector) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;

  std::set<std::string> resource_paths;
  resource_paths.insert(kResourceFileStrings);
  resource_paths.insert(kResourceFile100);
  resource_paths.insert(kResourceFile200);

  resource_provider::ResourceLoader resource_loader(connector, resource_paths);
  if (!resource_loader.BlockUntilLoaded())
    return;
  CHECK(resource_loader.loaded());
  ui::RegisterPathProvider();

  // Initialize resource bundle with 1x and 2x cursor bitmaps.
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      resource_loader.ReleaseFile(kResourceFileStrings),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      resource_loader.ReleaseFile(kResourceFile100), ui::SCALE_FACTOR_100P);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      resource_loader.ReleaseFile(kResourceFile200), ui::SCALE_FACTOR_200P);
}

void MandolineUIServicesApp::Initialize(mojo::Connector* connector,
                                        const mojo::Identity& identity,
                                        uint32_t id) {
  connector_ = connector;
  surfaces_state_ = new SurfacesState;

  base::PlatformThread::SetName("mus");

#if defined(USE_X11)
  XInitThreads();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUseX11TestConfig)) {
    ui::test::SetUseOverrideRedirectWindowByDefault(true);
  }
#endif

  InitializeResources(connector);

#if defined(USE_OZONE)
  // The ozone platform can provide its own event source. So initialize the
  // platform before creating the default event source.
  // TODO(rjkroege): Add tracing here.
  // Because GL libraries need to be initialized before entering the sandbox,
  // in MUS, |InitializeForUI| will load the GL libraries.
  ui::OzonePlatform::InitializeForUI();
#endif

// TODO(rjkroege): Enter sandbox here before we start threads in GpuState
// http://crbug.com/584532

#if !defined(OS_ANDROID)
  event_source_ = ui::PlatformEventSource::CreateDefault();
#endif

  // TODO(rjkroege): It is possible that we might want to generalize the
  // GpuState object.
  gpu_state_ = new GpuState();
  connection_manager_.reset(new ws::ConnectionManager(this, surfaces_state_));

  tracing_.Initialize(connector, identity.name());
}

bool MandolineUIServicesApp::AcceptConnection(Connection* connection) {
  connection->AddInterface<Gpu>(this);
  connection->AddInterface<mojom::DisplayManager>(this);
  connection->AddInterface<mojom::WindowManagerFactoryService>(this);
  connection->AddInterface<mojom::WindowTreeFactory>(this);
  connection->AddInterface<WindowTreeHostFactory>(this);
  return true;
}

void MandolineUIServicesApp::OnFirstDisplayReady() {
  PendingRequests requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    // TODO(sky): this needs to cache user id too!
    if (request->dm_request)
      Create(nullptr, std::move(*request->dm_request));
    else
      Create(nullptr, std::move(*request->wtf_request));
  }
}

void MandolineUIServicesApp::OnNoMoreDisplays() {
  base::MessageLoop::current()->QuitWhenIdle();
}

scoped_ptr<ws::WindowTreeBinding>
MandolineUIServicesApp::CreateWindowTreeBindingForEmbedAtWindow(
    ws::ConnectionManager* connection_manager,
    ws::WindowTree* tree,
    mojom::WindowTreeRequest tree_request,
    mojom::WindowTreeClientPtr client) {
  return make_scoped_ptr(new ws::DefaultWindowTreeBinding(
      tree, connection_manager, std::move(tree_request), std::move(client)));
}

void MandolineUIServicesApp::CreateDefaultDisplays() {
  // Display manages its own lifetime.
  ws::Display* host_impl = new ws::Display(
      connection_manager_.get(), connector_, gpu_state_, surfaces_state_);
  host_impl->Init(nullptr);
}

void MandolineUIServicesApp::Create(mojo::Connection* connection,
                                    mojom::DisplayManagerRequest request) {
  // TODO(sky): validate id.
  connection_manager_->display_manager()
      ->GetUserDisplayManager(connection->GetRemoteIdentity().user_id())
      ->AddDisplayManagerBinding(std::move(request));
}

void MandolineUIServicesApp::Create(
    mojo::Connection* connection,
    mojom::WindowManagerFactoryServiceRequest request) {
  connection_manager_->window_manager_factory_registry()->Register(
      connection->GetRemoteIdentity().user_id(), std::move(request));
}

void MandolineUIServicesApp::Create(Connection* connection,
                                    mojom::WindowTreeFactoryRequest request) {
  if (!connection_manager_->display_manager()->has_displays()) {
    scoped_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->wtf_request.reset(
        new mojo::InterfaceRequest<mojom::WindowTreeFactory>(
            std::move(request)));
    pending_requests_.push_back(std::move(pending_request));
    return;
  }
  if (!window_tree_factory_) {
    window_tree_factory_.reset(
        new ws::WindowTreeFactory(connection_manager_.get()));
  }
  window_tree_factory_->AddBinding(std::move(request));
}

void MandolineUIServicesApp::Create(
    Connection* connection,
    mojom::WindowTreeHostFactoryRequest request) {
  factory_bindings_.AddBinding(this, std::move(request));
}

void MandolineUIServicesApp::Create(mojo::Connection* connection,
                                    mojom::GpuRequest request) {
  DCHECK(gpu_state_);
  new GpuImpl(std::move(request), gpu_state_);
}

void MandolineUIServicesApp::CreateWindowTreeHost(
    mojom::WindowTreeHostRequest host,
    mojom::WindowTreeClientPtr tree_client) {
  DCHECK(connection_manager_);

  // TODO(fsamuel): We need to make sure that only the window manager can create
  // new roots.
  ws::Display* host_impl = new ws::Display(
      connection_manager_.get(), connector_, gpu_state_, surfaces_state_);

  scoped_ptr<ws::DisplayBindingImpl> display_binding(new ws::DisplayBindingImpl(
      std::move(host), host_impl, std::move(tree_client),
      connection_manager_.get()));
  host_impl->Init(std::move(display_binding));
}

}  // namespace mus
