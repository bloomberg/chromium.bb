// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_aura.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/aura/screen_orientation_listener.h"
#include "chrome/browser/ui/views/aura/screenshot_taker.h"
#include "ui/aura/root_window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/runtime_environment.h"
#endif

#if defined(USE_WEBKIT_COMPOSITOR)
#include "ui/gfx/compositor/compositor_setup.h"
#else
#include "ui/gfx/test/gfx_test_utils.h"
#endif

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura()
    : ChromeBrowserMainExtraParts() {
}

void ChromeBrowserMainExtraPartsAura::PreProfileInit() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestCompositor)) {
#if defined(USE_WEBKIT_COMPOSITOR)
    ui::SetupTestCompositor();
#else
    ui::gfx_test_utils::SetupTestCompositor();
#endif
  }

#if defined(OS_CHROMEOS)
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS())
    aura::RootWindow::set_use_fullscreen_host_window(true);
#endif

  // Shell takes ownership of ChromeShellDelegate.
  ash::Shell* shell = ash::Shell::CreateInstance(new ChromeShellDelegate);
  shell->accelerator_controller()->SetScreenshotDelegate(
      scoped_ptr<ash::ScreenshotDelegate>(new ScreenshotTaker).Pass());

  // Make sure the singleton ScreenOrientationListener object is created.
  ScreenOrientationListener::GetInstance();
}

void ChromeBrowserMainExtraPartsAura::PostMainMessageLoopRun() {
  ash::Shell::DeleteInstance();
  aura::RootWindow::DeleteInstance();
}
