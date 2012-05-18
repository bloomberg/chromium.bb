// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_ash.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/key_rewriter_event_filter.h"
#include "ash/wm/property_util.h"
#include "base/command_line.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/views/ash/caps_lock_handler.h"
#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/ash/key_rewriter.h"
#include "chrome/browser/ui/views/ash/screenshot_taker.h"
#include "chrome/browser/ui/views/ash/user_gesture_handler.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/user_gesture_client.h"
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

ChromeBrowserMainExtraPartsAsh::ChromeBrowserMainExtraPartsAsh()
    : ChromeBrowserMainExtraParts() {
}

ChromeBrowserMainExtraPartsAsh::~ChromeBrowserMainExtraPartsAsh() {
}

void ChromeBrowserMainExtraPartsAsh::PreProfileInit() {
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAuraHostWindowUseFullscreen)) {
    aura::MonitorManager::set_use_fullscreen_host_window(true);
    aura::RootWindow::set_hide_host_cursor(true);
    // Hide the mouse cursor completely at boot.
    if (!chromeos::UserManager::Get()->IsUserLoggedIn())
      ash::Shell::set_initially_hide_cursor(true);
  }
#endif

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

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableZeroBrowsersOpenForTests)) {
    browser::StartKeepAlive();
  }
#endif
  gesture_handler_.reset(new UserGestureHandler);
  aura::client::SetUserGestureClient(
      ash::Shell::GetRootWindow(), gesture_handler_.get());
  ash::Shell::GetRootWindow()->ShowRootWindow();
}

void ChromeBrowserMainExtraPartsAsh::PostProfileInit() {
}

void ChromeBrowserMainExtraPartsAsh::PostMainMessageLoopRun() {
  ash::Shell::DeleteInstance();
}
