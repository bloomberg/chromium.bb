// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_content_state.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

ChromeShellContentState::ChromeShellContentState() {}
ChromeShellContentState::~ChromeShellContentState() {}

content::BrowserContext* ChromeShellContentState::GetActiveBrowserContext() {
#if defined(OS_CHROMEOS)
  DCHECK(user_manager::UserManager::Get()->GetLoggedInUsers().size());
#endif
  return ProfileManager::GetActiveUserProfile();
}
