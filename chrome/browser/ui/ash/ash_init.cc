// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_init.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_switches.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "ash/wm/event_rewriter_event_filter.h"
#include "ash/wm/property_util.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/ash/event_rewriter.h"
#include "chrome/browser/ui/ash/screenshot_taker.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/compositor_setup.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/ash/brightness_controller_chromeos.h"
#include "chrome/browser/ui/ash/ime_controller_chromeos.h"
#include "chrome/browser/ui/ash/keyboard_brightness_controller_chromeos.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "ui/base/x/x11_util.h"
#endif


namespace chrome {

bool ShouldOpenAshOnStartup() {
#if defined(OS_CHROMEOS)
  return true;
#endif
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kOpenAsh);
}

void OpenAsh() {
  bool use_fullscreen = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAuraHostWindowUseFullscreen);

#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    use_fullscreen = true;
    // Hides the cursor outside of the Aura root window. The cursor will be
    // drawn within the Aura root window, and it'll remain hidden after the
    // Aura window is closed.
    ui::HideHostCursor();
  }
#endif

  if (use_fullscreen) {
    aura::DisplayManager::set_use_fullscreen_host_window(true);
#if defined(OS_CHROMEOS)
    // Hide the mouse cursor completely at boot.
    if (!chromeos::UserManager::Get()->IsUserLoggedIn())
      ash::Shell::set_initially_hide_cursor(true);
#endif
  }

  // Its easier to mark all windows as persisting and exclude the ones we care
  // about (browser windows), rather than explicitly excluding certain windows.
  ash::SetDefaultPersistsAcrossAllWorkspaces(true);

  // Shell takes ownership of ChromeShellDelegate.
  ash::Shell* shell = ash::Shell::CreateInstance(new ChromeShellDelegate);
  shell->event_rewriter_filter()->SetEventRewriterDelegate(
      scoped_ptr<ash::EventRewriterDelegate>(new EventRewriter).Pass());
  shell->accelerator_controller()->SetScreenshotDelegate(
      scoped_ptr<ash::ScreenshotDelegate>(new ScreenshotTaker).Pass());
#if defined(OS_CHROMEOS)
  shell->accelerator_controller()->SetBrightnessControlDelegate(
      scoped_ptr<ash::BrightnessControlDelegate>(
          new BrightnessController).Pass());
  shell->accelerator_controller()->SetImeControlDelegate(
      scoped_ptr<ash::ImeControlDelegate>(new ImeController).Pass());
  shell->accelerator_controller()->SetKeyboardBrightnessControlDelegate(
      scoped_ptr<ash::KeyboardBrightnessControlDelegate>(
          new KeyboardBrightnessController).Pass());
  ash::Shell::GetInstance()->high_contrast_controller()->SetEnabled(
      chromeos::accessibility::IsHighContrastEnabled());

  ash::Shell::GetInstance()->magnification_controller()->SetEnabled(
      chromeos::accessibility::IsScreenMagnifierEnabled());

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
    browser::StartKeepAlive();
  }
#endif
  ash::Shell::GetPrimaryRootWindow()->ShowRootWindow();
}

void CloseAsh() {
  if (ash::Shell::GetInstance())
    ash::Shell::DeleteInstance();
}

}  // namespace chrome
