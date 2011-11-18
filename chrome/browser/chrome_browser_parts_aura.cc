// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_parts_aura.h"
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/shell.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/runtime_environment.h"
#endif

ChromeBrowserPartsAura::ChromeBrowserPartsAura()
    : content::BrowserMainParts() {
}

void ChromeBrowserPartsAura::PreEarlyInitialization() {
}

void ChromeBrowserPartsAura::PostEarlyInitialization() {
}

void ChromeBrowserPartsAura::ToolkitInitialized() {
}

void ChromeBrowserPartsAura::PreMainMessageLoopStart() {
}

void ChromeBrowserPartsAura::PostMainMessageLoopStart() {
}

void ChromeBrowserPartsAura::PreMainMessageLoopRun() {
#if defined(OS_CHROMEOS)
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS())
    aura::Desktop::set_use_fullscreen_host_window(true);
#endif

  // Shell takes ownership of ChromeShellDelegate.
  aura_shell::Shell::CreateInstance(new ChromeShellDelegate);
}

bool ChromeBrowserPartsAura::MainMessageLoopRun(int* result_code) {
  return false;
}

void ChromeBrowserPartsAura::PostMainMessageLoopRun() {
}
