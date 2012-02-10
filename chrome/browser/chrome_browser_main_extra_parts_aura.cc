// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_aura.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/ui/views/aura/caps_lock_handler.h"
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/aura/screen_orientation_listener.h"
#include "chrome/browser/ui/views/aura/screenshot_taker.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/compositor/compositor_setup.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/aura/brightness_controller_chromeos.h"
#include "chrome/browser/ui/views/aura/volume_controller_chromeos.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#endif

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura()
    : ChromeBrowserMainExtraParts() {
}

void ChromeBrowserMainExtraPartsAura::PreProfileInit() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestCompositor)) {
    ui::SetupTestCompositor();
  }

#if defined(OS_CHROMEOS)
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS())
    aura::RootWindow::set_use_fullscreen_host_window(true);
#endif

  // Shell takes ownership of ChromeShellDelegate.
  ash::Shell* shell = ash::Shell::CreateInstance(new ChromeShellDelegate);
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
  shell->accelerator_controller()->SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate>(new VolumeController).Pass());
#endif

  // Make sure the singleton ScreenOrientationListener object is created.
  ScreenOrientationListener::GetInstance();
}

void ChromeBrowserMainExtraPartsAura::PostMainMessageLoopRun() {
  ash::Shell::DeleteInstance();
  aura::RootWindow::DeleteInstance();
  aura::Env::DeleteInstance();
}
