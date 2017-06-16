// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_init.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/accessibility_types.h"
#include "ash/aura/shell_port_classic.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/mus/bridge/shell_port_mash.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
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
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
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

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"  // nogncheck
#endif

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

std::unique_ptr<ash::mus::WindowManager> CreateMusShell() {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  const bool show_primary_host_on_connect = true;
  std::unique_ptr<ash::mus::WindowManager> window_manager =
      base::MakeUnique<ash::mus::WindowManager>(connector, ash::Config::MUS,
                                                show_primary_host_on_connect);
  // The WindowManager normally deletes the Shell when it loses its connection
  // to mus. Disable that by installing an empty callback. Chrome installs
  // its own callback to detect when the connection to mus is lost and that is
  // what shuts everything down.
  window_manager->SetLostConnectionCallback(base::BindOnce(&base::DoNothing));
  std::unique_ptr<aura::WindowTreeClient> window_tree_client =
      base::MakeUnique<aura::WindowTreeClient>(connector, window_manager.get(),
                                               window_manager.get());
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
#if defined(USE_X11)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Mus only runs on ozone.
    DCHECK_NE(ash::Config::MUS, chromeos::GetAshConfig());
    // Hides the cursor outside of the Aura root window. The cursor will be
    // drawn within the Aura root window, and it'll remain hidden after the
    // Aura window is closed.
    ui::HideHostCursor();
  }
#endif

  // Hide the mouse cursor completely at boot.
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    ash::Shell::set_initially_hide_cursor(true);

  // Balanced by a call to DestroyInstance() in CloseAsh() below.
  ash::ShellContentState::SetInstance(new ChromeShellContentState);

  if (chromeos::GetAshConfig() == ash::Config::MUS)
    window_manager_ = CreateMusShell();
  else
    CreateClassicShell();

  ash::Shell* shell = ash::Shell::Get();

  ash::AcceleratorControllerDelegateAura* accelerator_controller_delegate =
      nullptr;
  if (chromeos::GetAshConfig() == ash::Config::CLASSIC) {
    accelerator_controller_delegate =
        ash::ShellPortClassic::Get()->accelerator_controller_delegate();
  } else if (chromeos::GetAshConfig() == ash::Config::MUS) {
    accelerator_controller_delegate =
        ash::mus::ShellPortMash::Get()->accelerator_controller_delegate_mus();
  }
  if (accelerator_controller_delegate) {
    std::unique_ptr<ChromeScreenshotGrabber> screenshot_delegate =
        base::MakeUnique<ChromeScreenshotGrabber>();
    accelerator_controller_delegate->SetScreenshotDelegate(
        std::move(screenshot_delegate));
  }
  // TODO(flackr): Investigate exposing a blocking pool task runner to chromeos.
  chromeos::AccelerometerReader::GetInstance()->Initialize(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
  shell->high_contrast_controller()->SetEnabled(
      chromeos::AccessibilityManager::Get()->IsHighContrastEnabled());

  DCHECK(chromeos::MagnificationManager::Get());
  bool magnifier_enabled =
      chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  ash::MagnifierType magnifier_type =
      chromeos::MagnificationManager::Get()->GetMagnifierType();
  shell->magnification_controller()->SetEnabled(
      magnifier_enabled && magnifier_type == ash::MAGNIFIER_FULL);
  shell->partial_magnification_controller()->SetEnabled(
      magnifier_enabled && magnifier_type == ash::MAGNIFIER_PARTIAL);

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
