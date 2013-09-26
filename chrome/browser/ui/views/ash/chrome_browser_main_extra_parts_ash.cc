// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/ash/tab_scrubber.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"
#include "ui/keyboard/keyboard.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

#if defined(FILE_MANAGER_EXTENSION)
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"
#endif

#if !defined(OS_CHROMEOS)
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_delegate.h"
#endif

#if !defined(OS_CHROMEOS)
class ScreenTypeDelegateWin : public gfx::ScreenTypeDelegate {
 public:
  ScreenTypeDelegateWin() {}
  virtual gfx::ScreenType GetScreenTypeForNativeView(
      gfx::NativeView view) OVERRIDE {
    return chrome::IsNativeViewInAsh(view) ?
        gfx::SCREEN_TYPE_ALTERNATE :
        gfx::SCREEN_TYPE_NATIVE;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTypeDelegateWin);
};

class ShellDialogsDelegateWin : public ui::ShellDialogsDelegate {
 public:
  ShellDialogsDelegateWin() {}
  virtual bool IsWindowInMetro(gfx::NativeWindow window) OVERRIDE {
    return chrome::IsNativeViewInAsh(window);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDialogsDelegateWin);
};

base::LazyInstance<ShellDialogsDelegateWin> g_shell_dialogs_delegate;

#endif

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  if (chrome::ShouldOpenAshOnStartup()) {
    chrome::OpenAsh();
  } else {
#if !defined(OS_CHROMEOS)
    gfx::Screen::SetScreenTypeDelegate(new ScreenTypeDelegateWin);
    ui::SelectFileDialog::SetShellDialogsDelegate(
        &g_shell_dialogs_delegate.Get());
#endif
  }
#if defined(OS_CHROMEOS)
  // For OS_CHROMEOS, virtual keyboard needs to be initialized before profile
  // initialized. Otherwise, virtual keyboard extension will not load at login
  // screen.
  if (keyboard::IsKeyboardEnabled())
    keyboard::InitializeKeyboard();
#endif

#if defined(FILE_MANAGER_EXTENSION)
  ui::SelectFileDialog::SetFactory(new SelectFileDialogExtensionFactory);
#endif
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
  // Initialize TabScrubber after the Ash Shell has been initialized.
  if (ash::Shell::HasInstance())
    TabScrubber::GetInstance();
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  chrome::CloseAsh();
}
