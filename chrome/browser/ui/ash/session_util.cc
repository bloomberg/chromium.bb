// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_util.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

content::BrowserContext* GetActiveBrowserContext() {
#if defined(OS_CHROMEOS)
  DCHECK(user_manager::UserManager::Get()->GetLoggedInUsers().size());
#endif
  return ProfileManager::GetActiveUserProfile();
}

bool CanShowWindowForUser(
    aura::Window* window,
    const GetActiveBrowserContextCallback& get_context_callback) {
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  if (delegate->NumberOfLoggedInUsers() > 1) {
    content::BrowserContext* active_browser_context =
        get_context_callback.Run();
    content::BrowserContext* owner_browser_context =
        delegate->GetBrowserContextForWindow(window);
    content::BrowserContext* shown_browser_context =
        delegate->GetUserPresentingBrowserContextForWindow(window);

    if (owner_browser_context && active_browser_context &&
        owner_browser_context != active_browser_context &&
        shown_browser_context != active_browser_context) {
      return false;
    }
  }
  return true;
}
