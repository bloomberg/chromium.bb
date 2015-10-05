// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_content_state.h"

content::BrowserContext* ChromeShellContentState::GetBrowserContextByIndex(
    ash::UserIndex index) {
  return nullptr;
}

content::BrowserContext* ChromeShellContentState::GetBrowserContextForWindow(
    aura::Window* window) {
  return nullptr;
}

content::BrowserContext*
ChromeShellContentState::GetUserPresentingBrowserContextForWindow(
    aura::Window* window) {
  return nullptr;
}
