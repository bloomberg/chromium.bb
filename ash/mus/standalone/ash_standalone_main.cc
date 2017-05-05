// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/window_manager_application.h"
#include "ash/shell.h"
#include "ash/shell/example_app_list_presenter.h"
#include "ash/shell/example_session_controller_client.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/window_type_launcher.h"
#include "ash/shell/window_watcher.h"
#include "ash/shell_observer.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/ui/public/cpp/input_devices/input_device_client.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/examples/examples_window.h"

namespace ash {
namespace {

void ShowViewsExamples() {
  views::examples::ShowExamplesWindow(views::examples::DO_NOTHING_ON_CLOSE);
}

class ShellInit : public shell::ShellDelegateImpl, public ShellObserver {
 public:
  ShellInit()
      : input_device_client_(base::MakeUnique<ui::InputDeviceClient>()) {}
  ~ShellInit() override = default;

  void set_window_manager_app(mus::WindowManagerApplication* app) {
    window_manager_app_ = app;
  }

  // shell::ShellDelegateImpl:
  void PreInit() override {
    DCHECK(window_manager_app_->GetConnector());
    ui::mojom::InputDeviceServerPtr server;
    window_manager_app_->GetConnector()->BindInterface(ui::mojom::kServiceName,
                                                       &server);
    input_device_client_->Connect(std::move(server));

    shell::ShellDelegateImpl::PreInit();
    Shell::Get()->AddShellObserver(this);
  }

  void PreShutdown() override {
    display::Screen::GetScreen()->RemoveObserver(window_watcher_.get());
  }

  // ShellObserver:
  void OnShellInitialized() override {
    Shell::Get()->RemoveShellObserver(this);

    // Initialize session controller client and create fake user sessions. The
    // fake user sessions makes ash into the logged in state.
    example_session_controller_client_ =
        base::MakeUnique<shell::ExampleSessionControllerClient>(
            Shell::Get()->session_controller());
    example_session_controller_client_->Initialize();

    window_watcher_ = base::MakeUnique<shell::WindowWatcher>();
    display::Screen::GetScreen()->AddObserver(window_watcher_.get());
    shell::InitWindowTypeLauncher(base::Bind(&ShowViewsExamples));

    // Initialize the example app list presenter.
    example_app_list_presenter_ =
        base::MakeUnique<shell::ExampleAppListPresenter>();
    Shell::Get()->app_list()->SetAppListPresenter(
        example_app_list_presenter_->CreateInterfacePtrAndBind());

    Shell::GetPrimaryRootWindow()->GetHost()->Show();
  }

 private:
  std::unique_ptr<shell::ExampleAppListPresenter> example_app_list_presenter_;
  // Used to observe new windows and update the shelf accordingly.
  std::unique_ptr<shell::WindowWatcher> window_watcher_;
  std::unique_ptr<shell::ExampleSessionControllerClient>
      example_session_controller_client_;
  std::unique_ptr<ui::InputDeviceClient> input_device_client_;
  mus::WindowManagerApplication* window_manager_app_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellInit);
};

}  // namespace
}  // namespace ash

MojoResult ServiceMain(MojoHandle service_request_handle) {
  const bool show_primary_host_on_connect = false;
  std::unique_ptr<ash::ShellInit> shell_init_ptr =
      base::MakeUnique<ash::ShellInit>();
  ash::ShellInit* shell_init = shell_init_ptr.get();
  ash::mus::WindowManagerApplication* window_manager_app =
      new ash::mus::WindowManagerApplication(show_primary_host_on_connect,
                                             ash::Config::MUS,
                                             std::move(shell_init_ptr));
  shell_init->set_window_manager_app(window_manager_app);
  service_manager::ServiceRunner runner(window_manager_app);
  return runner.Run(service_request_handle);
}
