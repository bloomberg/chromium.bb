// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"

#include "ash/ash_switches.h"
#include "ash/multi_profile_uma.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_stub.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"

namespace {
chrome::MultiUserWindowManager* g_instance = NULL;
}  // namespace

namespace chrome {

// Caching the current multi profile mode to avoid expensive detection
// operations.
chrome::MultiUserWindowManager::MultiProfileMode
    chrome::MultiUserWindowManager::multi_user_mode_ =
        chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_UNINITIALIZED;

// static
MultiUserWindowManager* MultiUserWindowManager::GetInstance() {
  return g_instance;
}

MultiUserWindowManager* MultiUserWindowManager::CreateInstance() {
  DCHECK(!g_instance);
  multi_user_mode_ = MULTI_PROFILE_MODE_OFF;
  ash::MultiProfileUMA::SessionMode mode =
      ash::MultiProfileUMA::SESSION_SINGLE_USER_MODE;
  if (!g_instance &&
      ash::Shell::GetInstance()->delegate()->IsMultiProfilesEnabled()) {
    MultiUserWindowManagerChromeOS* manager =
        new MultiUserWindowManagerChromeOS(ash::Shell::GetInstance()
                                               ->session_state_delegate()
                                               ->GetUserInfo(0)
                                               ->GetAccountId());
    g_instance = manager;
    manager->Init();
    multi_user_mode_ = MULTI_PROFILE_MODE_SEPARATED;
    mode = ash::MultiProfileUMA::SESSION_SEPARATE_DESKTOP_MODE;
  } else if (ash::Shell::GetInstance()->delegate()->IsMultiProfilesEnabled()) {
    // The side by side mode is using the Single user window manager since all
    // windows are unmanaged side by side.
    multi_user_mode_ = MULTI_PROFILE_MODE_MIXED;
    mode = ash::MultiProfileUMA::SESSION_SIDE_BY_SIDE_MODE;
  }
  ash::MultiProfileUMA::RecordSessionMode(mode);

  // If there was no instance created yet we create a dummy instance.
  if (!g_instance)
    g_instance = new MultiUserWindowManagerStub();

  return g_instance;
}

// static
MultiUserWindowManager::MultiProfileMode
MultiUserWindowManager::GetMultiProfileMode() {
  return multi_user_mode_;
}

// satic
bool MultiUserWindowManager::ShouldShowAvatar(aura::Window* window) {
  // Note: In case of the M-31 mode the window manager won't exist.
  if (GetMultiProfileMode() == MULTI_PROFILE_MODE_SEPARATED) {
    // If the window is shown on a different desktop than the user, it should
    // have the avatar icon
    MultiUserWindowManager* instance = GetInstance();
    return !instance->IsWindowOnDesktopOfUser(window,
                                              instance->GetWindowOwner(window));
  }
  return false;
}

// static
void MultiUserWindowManager::DeleteInstance() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = NULL;
  multi_user_mode_ = MULTI_PROFILE_MODE_UNINITIALIZED;
}

void MultiUserWindowManager::SetInstanceForTest(
    MultiUserWindowManager* instance,
    MultiProfileMode mode) {
  if (g_instance)
    DeleteInstance();
  g_instance = instance;
  multi_user_mode_ = mode;
}

}  // namespace chrome
