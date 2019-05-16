// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_browser_main_parts.h"

#include <memory>
#include <utility>

#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/login_status.h"
#include "ash/shell.h"
#include "ash/shell/content/embedded_browser.h"
#include "ash/shell/example_app_list_client.h"
#include "ash/shell/example_session_controller_client.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/shell_views_delegate.h"
#include "ash/shell/window_type_launcher.h"
#include "ash/shell/window_watcher.h"
#include "ash/shell_init_params.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/exo/file_helper.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_net_log.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "net/base/net_module.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/views/examples/examples_window_with_content.h"
#include "ui/wm/core/wm_state.h"

namespace ash {
namespace shell {

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters) {}

ShellBrowserMainParts::~ShellBrowserMainParts() = default;

void ShellBrowserMainParts::PreMainMessageLoopStart() {}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
  chromeos::PowerManagerClient::InitializeFake();
  chromeos::BiodClient::InitializeFake();
}

void ShellBrowserMainParts::ToolkitInitialized() {
  wm_state_.reset(new ::wm::WMState);
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new content::ShellNetLog("ash_shell"));
  browser_context_.reset(
      new content::ShellBrowserContext(false, net_log_.get()));

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_ = std::make_unique<ShellViewsDelegate>();

  // Create CrasAudioHandler for testing since g_browser_process
  // is absent.
  chromeos::CrasAudioHandler::InitializeForTesting();

  bluez::BluezDBusManager::InitializeFake();

  chromeos::PowerPolicyController::Initialize(
      chromeos::PowerManagerClient::Get());

  service_manager::Connector* const connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();

  ui::MaterialDesignController::Initialize();
  ash::ShellInitParams init_params;
  init_params.delegate = std::make_unique<ash::shell::ShellDelegateImpl>();
  init_params.context_factory = content::GetContextFactory();
  init_params.context_factory_private = content::GetContextFactoryPrivate();
  init_params.connector = connector;
  init_params.keyboard_ui_factory = std::make_unique<TestKeyboardUIFactory>();
  ash::Shell::CreateInstance(std::move(init_params));

  // Initialize session controller client and create fake user sessions. The
  // fake user sessions makes ash into the logged in state.
  example_session_controller_client_ =
      std::make_unique<ExampleSessionControllerClient>(
          Shell::Get()->session_controller());
  example_session_controller_client_->Initialize();

  window_watcher_ = std::make_unique<WindowWatcher>();

  ash::shell::InitWindowTypeLauncher(
      base::BindRepeating(&views::examples::ShowExamplesWindowWithContent,
                          base::Passed(base::OnceClosure()),
                          base::Unretained(browser_context_.get()), nullptr),
      base::BindRepeating(&EmbeddedBrowser::Create,
                          base::Unretained(browser_context_.get()),
                          GURL("https://www.google.com")));

  example_app_list_client_ = std::make_unique<ExampleAppListClient>(
      Shell::Get()->app_list_controller());

  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();

  ash::Shell::Get()->InitWaylandServer(nullptr);
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  window_watcher_.reset();
  ash::Shell::DeleteInstance();

  chromeos::CrasAudioHandler::Shutdown();

  chromeos::PowerPolicyController::Shutdown();

  views_delegate_.reset();

  // The keyboard may have created a WebContents. The WebContents is destroyed
  // with the UI, and it needs the BrowserContext to be alive during its
  // destruction. So destroy all of the UI elements before destroying the
  // browser context.
  browser_context_.reset();
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::RunLoop run_loop;
  example_session_controller_client_->set_quit_closure(
      run_loop.QuitWhenIdleClosure());
  run_loop.Run();
  return true;
}

}  // namespace shell
}  // namespace ash
