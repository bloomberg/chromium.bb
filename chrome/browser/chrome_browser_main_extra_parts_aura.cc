// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_aura.h"
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/shell.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/runtime_environment.h"
#endif

ChromeBrowserMainExtraPartsAura::ChromeBrowserMainExtraPartsAura()
    : ChromeBrowserMainExtraParts() {
}

void ChromeBrowserMainExtraPartsAura::PostBrowserProcessInit() {
#if defined(OS_CHROMEOS)
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS())
    aura::Desktop::set_use_fullscreen_host_window(true);
#endif

  // Shell takes ownership of ChromeShellDelegate.
  aura_shell::Shell::CreateInstance(new ChromeShellDelegate);
}
