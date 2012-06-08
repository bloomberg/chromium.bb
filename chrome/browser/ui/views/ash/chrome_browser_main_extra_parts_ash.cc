// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_switches.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/shell.h"
#include "ash/wm/key_rewriter_event_filter.h"
#include "ash/wm/property_util.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/toolkit_extra_parts.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/views/ash/caps_lock_handler.h"
#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/ash/key_rewriter.h"
#include "chrome/browser/ui/views/ash/screenshot_taker.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/compositor_setup.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/views/ash/brightness_controller_chromeos.h"
#include "chrome/browser/ui/views/ash/ime_controller_chromeos.h"
#include "chrome/browser/ui/views/ash/volume_controller_chromeos.h"
#endif

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
  bool use_fullscreen = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAuraHostWindowUseFullscreen);

#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS())
    use_fullscreen = true;
#endif

  if (use_fullscreen) {
    aura::MonitorManager::set_use_fullscreen_host_window(true);
#if defined(OS_CHROMEOS)
    aura::RootWindow::set_hide_host_cursor(true);
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
  shell->key_rewriter_filter()->SetKeyRewriterDelegate(
      scoped_ptr<ash::KeyRewriterDelegate>(new KeyRewriter).Pass());
  shell->accelerator_controller()->SetScreenshotDelegate(
      scoped_ptr<ash::ScreenshotDelegate>(new ScreenshotTaker).Pass());
#if defined(OS_CHROMEOS)
  shell->accelerator_controller()->SetBrightnessControlDelegate(
      scoped_ptr<ash::BrightnessControlDelegate>(
          new BrightnessController).Pass());
  chromeos::input_method::XKeyboard* xkeyboard =
      chromeos::input_method::InputMethodManager::GetInstance()->GetXKeyboard();
  shell->accelerator_controller()->SetCapsLockDelegate(
      scoped_ptr<ash::CapsLockDelegate>(new CapsLockHandler(xkeyboard)).Pass());
  shell->accelerator_controller()->SetImeControlDelegate(
      scoped_ptr<ash::ImeControlDelegate>(new ImeController).Pass());
  shell->accelerator_controller()->SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate>(new VolumeController).Pass());

  ash::Shell::GetInstance()->high_contrast_controller()->SetEnabled(
      chromeos::accessibility::IsHighContrastEnabled());

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
    browser::StartKeepAlive();
  }
#endif
  ash::Shell::GetPrimaryRootWindow()->ShowRootWindow();
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  ash::Shell::DeleteInstance();
}

namespace browser {

void AddAshToolkitExtraParts(ChromeBrowserMainParts* main_parts) {
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAsh());
}

}  // namespace browser
