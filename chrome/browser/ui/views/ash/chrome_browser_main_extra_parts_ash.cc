// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_mus.h"
#include "chrome/browser/ui/views/ash/tab_scrubber.h"
#include "chrome/browser/ui/views/frame/immersive_context_mus.h"
#include "chrome/browser/ui/views/frame/immersive_handler_factory_mus.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/env.h"
#include "ui/keyboard/content/keyboard.h"
#include "ui/keyboard/keyboard_controller.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/vpn_list_forwarder.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#endif  // defined(OS_CHROMEOS)

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (chrome::ShouldOpenAshOnStartup())
    chrome::OpenAsh(gfx::kNullAcceleratedWidget);

  if (chrome::IsRunningInMash()) {
    immersive_context_ = base::MakeUnique<ImmersiveContextMus>();
    immersive_handler_factory_ = base::MakeUnique<ImmersiveHandlerFactoryMus>();
  }

#if defined(OS_CHROMEOS)
  // Must be available at login screen, so initialize before profile.
  system_tray_client_ = base::MakeUnique<SystemTrayClient>();

  vpn_list_forwarder_ = base::MakeUnique<VpnListForwarder>();

  // For OS_CHROMEOS, virtual keyboard needs to be initialized before profile
  // initialized. Otherwise, virtual keyboard extension will not load at login
  // screen.
  keyboard::InitializeKeyboard();

  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);
#endif  // defined(OS_CHROMEOS)
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
  if (chrome::IsRunningInMash()) {
    DCHECK(!ash::Shell::HasInstance());
    DCHECK(!ChromeLauncherController::instance());
    chrome_launcher_controller_mus_ =
        base::MakeUnique<ChromeLauncherControllerMus>();
    chrome_launcher_controller_mus_->Init();
  }

  if (!ash::Shell::HasInstance())
    return;

  // Initialize TabScrubber after the Ash Shell has been initialized.
  TabScrubber::GetInstance();
  // Activate virtual keyboard after profile is initialized. It depends on the
  // default profile.
  ash::Shell::GetPrimaryRootWindowController()->ActivateKeyboard(
      keyboard::KeyboardController::GetInstance());
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
#if defined(OS_CHROMEOS)
  vpn_list_forwarder_.reset();
  system_tray_client_.reset();
#endif
  chrome::CloseAsh();
}
