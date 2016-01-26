// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/ash/tab_scrubber.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"
#include "ui/keyboard/content/keyboard.h"
#include "ui/keyboard/keyboard_controller.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#endif

#if !defined(OS_CHROMEOS)
class ScreenTypeDelegateWin : public gfx::ScreenTypeDelegate {
 public:
  ScreenTypeDelegateWin() {}
  gfx::ScreenType GetScreenTypeForNativeView(gfx::NativeView view) override {
    return chrome::IsNativeViewInAsh(view) ?
        gfx::SCREEN_TYPE_ALTERNATE :
        gfx::SCREEN_TYPE_NATIVE;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTypeDelegateWin);
};
#endif  // !OS_CHROMEOS

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (chrome::ShouldOpenAshOnStartup()) {
    chrome::OpenAsh(gfx::kNullAcceleratedWidget);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ash::Shell::GetInstance()->CreateShelf();
  ash::Shell::GetInstance()->ShowShelf();
#endif
  } else {
#if !defined(OS_CHROMEOS)
    gfx::Screen::SetScreenTypeDelegate(new ScreenTypeDelegateWin);
#endif
  }
#if defined(OS_CHROMEOS)
  // For OS_CHROMEOS, virtual keyboard needs to be initialized before profile
  // initialized. Otherwise, virtual keyboard extension will not load at login
  // screen.
  keyboard::InitializeKeyboard();
#endif

#if defined(OS_CHROMEOS)
  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);
#endif
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
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
  chrome::CloseAsh();
}
