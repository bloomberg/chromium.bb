// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/mus_app.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/mus/common/args.h"
#include "components/mus/gles2/gpu_impl.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/user_display_manager.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_server_test_impl.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "components/mus/ws/window_tree_factory.h"
#include "components/mus/ws/window_tree_host_factory.h"
#include "mojo/public/c/system/main.h"
#include "services/catalog/public/cpp/resource_loader.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/tracing/public/cpp/tracing_impl.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/platform_window/x11/x11_window.h"
#elif defined(USE_OZONE)
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

using shell::Connection;
using mojo::InterfaceRequest;
using mus::mojom::Gpu;
using mus::mojom::WindowServerTest;
using mus::mojom::WindowTreeHostFactory;

namespace mus {

namespace {

const char kResourceFileStrings[] = "mus_app_resources_strings.pak";
const char kResourceFile100[] = "mus_app_resources_100.pak";
const char kResourceFile200[] = "mus_app_resources_200.pak";

}  // namespace

// TODO(sky): this is a pretty typical pattern, make it easier to do.
struct MandolineUIServicesApp::PendingRequest {
  shell::Connection* connection;
  std::unique_ptr<mojo::InterfaceRequest<mojom::WindowTreeFactory>> wtf_request;
};

struct MandolineUIServicesApp::UserState {
  std::unique_ptr<ws::WindowTreeHostFactory> window_tree_host_factory;
};

MandolineUIServicesApp::MandolineUIServicesApp() : test_config_(false) {}

MandolineUIServicesApp::~MandolineUIServicesApp() {
  // Destroy |window_server_| first, since it depends on |event_source_|.
  // WindowServer (or more correctly its Displays) may have state that needs to
  // be destroyed before GpuState as well.
  window_server_.reset();

  if (platform_display_init_params_.gpu_state)
    platform_display_init_params_.gpu_state->StopThreads();
}

void MandolineUIServicesApp::InitializeResources(shell::Connector* connector) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;

  std::set<std::string> resource_paths;
  resource_paths.insert(kResourceFileStrings);
  resource_paths.insert(kResourceFile100);
  resource_paths.insert(kResourceFile200);

  catalog::ResourceLoader loader;
  filesystem::DirectoryPtr directory;
  connector->ConnectToInterface("mojo:catalog", &directory);
  CHECK(loader.OpenFiles(std::move(directory), resource_paths));

  ui::RegisterPathProvider();

  // Initialize resource bundle with 1x and 2x cursor bitmaps.
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      loader.TakeFile(kResourceFileStrings),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  rb.AddDataPackFromFile(loader.TakeFile(kResourceFile100),
                         ui::SCALE_FACTOR_100P);
  rb.AddDataPackFromFile(loader.TakeFile(kResourceFile200),
                         ui::SCALE_FACTOR_200P);
}

MandolineUIServicesApp::UserState* MandolineUIServicesApp::GetUserState(
    shell::Connection* connection) {
  const ws::UserId& user_id = connection->GetRemoteIdentity().user_id();
  auto it = user_id_to_user_state_.find(user_id);
  if (it != user_id_to_user_state_.end())
    return it->second.get();
  user_id_to_user_state_[user_id] = base::WrapUnique(new UserState);
  return user_id_to_user_state_[user_id].get();
}

void MandolineUIServicesApp::AddUserIfNecessary(shell::Connection* connection) {
  window_server_->user_id_tracker()->AddUserId(
      connection->GetRemoteIdentity().user_id());
}

void MandolineUIServicesApp::Initialize(shell::Connector* connector,
                                        const shell::Identity& identity,
                                        uint32_t id) {
  platform_display_init_params_.connector = connector;
  platform_display_init_params_.surfaces_state = new SurfacesState;

  base::PlatformThread::SetName("mus");
  tracing_.Initialize(connector, identity.name());
  TRACE_EVENT0("mus", "MandolineUIServicesApp::Initialize started");

  test_config_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kUseTestConfig);
#if defined(USE_X11)
  XInitThreads();
  if (test_config_)
    ui::test::SetUseOverrideRedirectWindowByDefault(true);
#endif

  InitializeResources(connector);

#if defined(USE_OZONE)
  // The ozone platform can provide its own event source. So initialize the
  // platform before creating the default event source.
  // TODO(rjkroege): Add tracing here.
  // Because GL libraries need to be initialized before entering the sandbox,
  // in MUS, |InitializeForUI| will load the GL libraries.
  ui::OzonePlatform::InitializeForUI();

  // TODO(kylechar): We might not always want a US keyboard layout.
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");
  client_native_pixmap_factory_ = ui::ClientNativePixmapFactory::Create();
  ui::ClientNativePixmapFactory::SetInstance(
      client_native_pixmap_factory_.get());

  DCHECK(ui::ClientNativePixmapFactory::GetInstance());
#endif

// TODO(rjkroege): Enter sandbox here before we start threads in GpuState
// http://crbug.com/584532

#if !defined(OS_ANDROID)
  event_source_ = ui::PlatformEventSource::CreateDefault();
#endif

  // TODO(rjkroege): It is possible that we might want to generalize the
  // GpuState object.
  platform_display_init_params_.gpu_state = new GpuState();
  window_server_.reset(
      new ws::WindowServer(this, platform_display_init_params_.surfaces_state));
}

bool MandolineUIServicesApp::AcceptConnection(Connection* connection) {
  connection->AddInterface<Gpu>(this);
  connection->AddInterface<mojom::DisplayManager>(this);
  connection->AddInterface<mojom::UserAccessManager>(this);
  connection->AddInterface<WindowTreeHostFactory>(this);
  connection->AddInterface<mojom::WindowManagerFactoryService>(this);
  connection->AddInterface<mojom::WindowTreeFactory>(this);
  if (test_config_)
    connection->AddInterface<WindowServerTest>(this);
  return true;
}

void MandolineUIServicesApp::OnFirstDisplayReady() {
  PendingRequests requests;
  requests.swap(pending_requests_);
  for (auto& request : requests)
    Create(request->connection, std::move(*request->wtf_request));
}

void MandolineUIServicesApp::OnNoMoreDisplays() {
  // We may get here from the destructor, in which case there is no messageloop.
  if (base::MessageLoop::current())
    base::MessageLoop::current()->QuitWhenIdle();
}

bool MandolineUIServicesApp::IsTestConfig() const {
  return test_config_;
}

void MandolineUIServicesApp::CreateDefaultDisplays() {
  // Display manages its own lifetime.
  ws::Display* host_impl =
      new ws::Display(window_server_.get(), platform_display_init_params_);
  host_impl->Init(nullptr);
}

void MandolineUIServicesApp::Create(shell::Connection* connection,
                                    mojom::DisplayManagerRequest request) {
  window_server_->display_manager()
      ->GetUserDisplayManager(connection->GetRemoteIdentity().user_id())
      ->AddDisplayManagerBinding(std::move(request));
}

void MandolineUIServicesApp::Create(shell::Connection* connection,
                                    mojom::UserAccessManagerRequest request) {
  window_server_->user_id_tracker()->Bind(std::move(request));
}

void MandolineUIServicesApp::Create(
    shell::Connection* connection,
    mojom::WindowManagerFactoryServiceRequest request) {
  AddUserIfNecessary(connection);
  window_server_->window_manager_factory_registry()->Register(
      connection->GetRemoteIdentity().user_id(), std::move(request));
}

void MandolineUIServicesApp::Create(Connection* connection,
                                    mojom::WindowTreeFactoryRequest request) {
  AddUserIfNecessary(connection);
  if (!window_server_->display_manager()->has_displays()) {
    std::unique_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->connection = connection;
    pending_request->wtf_request.reset(
        new mojo::InterfaceRequest<mojom::WindowTreeFactory>(
            std::move(request)));
    pending_requests_.push_back(std::move(pending_request));
    return;
  }
  AddUserIfNecessary(connection);
  new ws::WindowTreeFactory(
      window_server_.get(), connection->GetRemoteIdentity().user_id(),
      connection->GetRemoteIdentity().name(), std::move(request));
}

void MandolineUIServicesApp::Create(
    Connection* connection,
    mojom::WindowTreeHostFactoryRequest request) {
  UserState* user_state = GetUserState(connection);
  if (!user_state->window_tree_host_factory) {
    user_state->window_tree_host_factory.reset(new ws::WindowTreeHostFactory(
        window_server_.get(), connection->GetRemoteIdentity().user_id(),
        platform_display_init_params_));
  }
  user_state->window_tree_host_factory->AddBinding(std::move(request));
}

void MandolineUIServicesApp::Create(Connection* connection,
                                    mojom::WindowServerTestRequest request) {
  if (!test_config_)
    return;
  new ws::WindowServerTestImpl(window_server_.get(), std::move(request));
}

void MandolineUIServicesApp::Create(shell::Connection* connection,
                                    mojom::GpuRequest request) {
  DCHECK(platform_display_init_params_.gpu_state);
  new GpuImpl(std::move(request), platform_display_init_params_.gpu_state);
}

}  // namespace mus
