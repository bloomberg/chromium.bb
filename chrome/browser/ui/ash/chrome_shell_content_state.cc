// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_content_state.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

ChromeShellContentState* g_instance = nullptr;

}  // namespace

// static
ChromeShellContentState* ChromeShellContentState::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

ChromeShellContentState::ChromeShellContentState() {
  DCHECK(!g_instance);
  g_instance = this;
}

ChromeShellContentState::~ChromeShellContentState() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

content::BrowserContext* ChromeShellContentState::GetActiveBrowserContext() {
  DCHECK(user_manager::UserManager::Get()->GetLoggedInUsers().size());
  return ProfileManager::GetActiveUserProfile();
}
