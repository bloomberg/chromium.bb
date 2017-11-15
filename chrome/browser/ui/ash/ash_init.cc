// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_init.h"

#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/accessibility_types.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/chrome_shell_content_state.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/service_manager_connection.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window_tree_host.h"

namespace {

void CreateClassicShell() {
  ash::ShellInitParams shell_init_params;
  // Shell takes ownership of ChromeShellDelegate.
  shell_init_params.delegate = new ChromeShellDelegate;
  shell_init_params.context_factory = content::GetContextFactory();
  shell_init_params.context_factory_private =
      content::GetContextFactoryPrivate();

  ash::Shell::CreateInstance(shell_init_params);
}

std::unique_ptr<ash::WindowManager> CreateMusShell() {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  const bool show_primary_host_on_connect = true;
  std::unique_ptr<ash::WindowManager> window_manager =
      base::MakeUnique<ash::WindowManager>(connector, ash::Config::MUS,
                                           show_primary_host_on_connect);
  // The WindowManager normally deletes the Shell when it loses its connection
  // to mus. Disable that by installing an empty callback. Chrome installs
  // its own callback to detect when the connection to mus is lost and that is
  // what shuts everything down.
  window_manager->SetLostConnectionCallback(base::BindOnce(&base::DoNothing));
  // When Ash runs in the same services as chrome content creates the
  // DiscardableSharedMemoryManager.
  const bool create_discardable_memory = false;
  std::unique_ptr<aura::WindowTreeClient> window_tree_client =
      base::MakeUnique<aura::WindowTreeClient>(
          connector, window_manager.get(), window_manager.get(), nullptr,
          nullptr, create_discardable_memory);
  const bool automatically_create_display_roots = false;
  window_tree_client->ConnectAsWindowManager(
      automatically_create_display_roots);
  aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client.get());
  window_manager->Init(std::move(window_tree_client),
                       base::MakeUnique<ChromeShellDelegate>());
  CHECK(window_manager->WaitForInitialDisplays());
  return window_manager;
}

}  // namespace

AshInit::AshInit() {
  // Hide the mouse cursor completely at boot.
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    ash::Shell::set_initially_hide_cursor(true);

  // Balanced by a call to DestroyInstance() in CloseAsh() below.
  ash::ShellContentState::SetInstance(new ChromeShellContentState);

  if (chromeos::GetAshConfig() == ash::Config::MUS)
    window_manager_ = CreateMusShell();
  else
    CreateClassicShell();

  // TODO(flackr): Investigate exposing a blocking pool task runner to chromeos.
  chromeos::AccelerometerReader::GetInstance()->Initialize(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  ash::Shell* shell = ash::Shell::Get();
  shell->high_contrast_controller()->SetEnabled(
      chromeos::AccessibilityManager::Get()->IsHighContrastEnabled());

  DCHECK(chromeos::MagnificationManager::Get());
  shell->magnification_controller()->SetEnabled(
      chromeos::MagnificationManager::Get()->IsMagnifierEnabled());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableZeroBrowsersOpenForTests)) {
    g_browser_process->platform_part()->RegisterKeepAlive();
  }
  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

AshInit::~AshInit() {
  // |window_manager_| deletes the Shell.
  if (!window_manager_ && ash::Shell::HasInstance()) {
    ash::Shell::DeleteInstance();
    ash::ShellContentState::DestroyInstance();
  }
}
