// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"

#include "ash/multi_profile_uma.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_stub.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"

namespace {
MultiUserWindowManager* g_instance = nullptr;
}  // namespace

// static
MultiUserWindowManager* MultiUserWindowManager::GetInstance() {
  return g_instance;
}

MultiUserWindowManager* MultiUserWindowManager::CreateInstance() {
  DCHECK(!g_instance);
  ash::MultiProfileUMA::SessionMode mode =
      ash::MultiProfileUMA::SESSION_SINGLE_USER_MODE;
  // TODO(crbug.com/557406): Enable this component in Mash. The object itself
  // has direct ash dependencies.
  if (!ash_util::IsRunningInMash() &&
      SessionControllerClient::IsMultiProfileAvailable()) {
    MultiUserWindowManagerChromeOS* manager =
        new MultiUserWindowManagerChromeOS(
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
    g_instance = manager;
    manager->Init();
    mode = ash::MultiProfileUMA::SESSION_SEPARATE_DESKTOP_MODE;
  }
  ash::MultiProfileUMA::RecordSessionMode(mode);

  // If there was no instance created yet we create a dummy instance.
  if (!g_instance)
    g_instance = new MultiUserWindowManagerStub();

  return g_instance;
}

// static
bool MultiUserWindowManager::ShouldShowAvatar(aura::Window* window) {
  // Session restore can open a window for the first user before the instance
  // is created.
  if (!g_instance)
    return false;

  // Show the avatar icon if the window is on a different desktop than the
  // window's owner's desktop. The stub implementation does the right thing
  // for single-user mode.
  return !g_instance->IsWindowOnDesktopOfUser(
      window, g_instance->GetWindowOwner(window));
}

// static
void MultiUserWindowManager::DeleteInstance() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

void MultiUserWindowManager::SetInstanceForTest(
    MultiUserWindowManager* instance) {
  if (g_instance)
    DeleteInstance();
  g_instance = instance;
}
