// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"

#include <vector>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "components/user_manager/user_manager.h"
#endif

namespace multi_user_util {

AccountId GetAccountIdFromProfile(Profile* profile) {
#if defined(OS_CHROMEOS)
  // If in a session the refresh token is revoked, GetProfileUserName() might
  // returns an empty user email which will cause GetAccountIdFromProfile()
  // returns an empty account id, which might cause weird behaviors or crash.
  // Note: If the refresh token is revoked because the user changes his GAIA
  // password, we will force log out the user within 120 seconds. See crbug.com/
  // 587318 for more detail.
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          profile->GetOriginalProfile());
  return user ? user->GetAccountId() : EmptyAccountId();
#else
  return GetAccountIdFromEmail(
      profile->GetOriginalProfile()->GetProfileUserName());
#endif
}

AccountId GetAccountIdFromEmail(const std::string& email) {
  // |email| and profile name could be empty if not yet logged in or guest mode.
  return email.empty() ? EmptyAccountId()
                       : AccountId::FromUserEmail(gaia::CanonicalizeEmail(
                             gaia::SanitizeEmail(email)));
}

Profile* GetProfileFromAccountId(const AccountId& account_id) {
  // Unit tests can end up here without a |g_browser_process|.
  if (!g_browser_process || !g_browser_process->profile_manager())
    return nullptr;

  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();

  std::vector<Profile*>::const_iterator profile_iterator = profiles.begin();
  for (; profile_iterator != profiles.end(); ++profile_iterator) {
    if (GetAccountIdFromProfile(*profile_iterator) == account_id)
      return *profile_iterator;
  }

  return nullptr;
}

Profile* GetProfileFromWindow(aura::Window* window) {
#if defined(OS_CHROMEOS)
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();
  // We might come here before the manager got created - or in a unit test.
  if (!manager)
    return nullptr;
  const AccountId account_id = manager->GetUserPresentingWindow(window);
  return account_id.is_valid() ? GetProfileFromAccountId(account_id) : nullptr;
#else
  return nullptr;
#endif
}

bool IsProfileFromActiveUser(Profile* profile) {
#if defined(OS_CHROMEOS)
  return GetAccountIdFromProfile(profile) ==
         user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
#else
  // In non Chrome OS configurations this will be always true since this only
  // makes sense in separate desktop mode.
  return true;
#endif
}

const AccountId GetCurrentAccountId() {
#if defined(OS_CHROMEOS)
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  // In unit tests user login phase is usually skipped.
  if (user)
    return user->GetAccountId();
#endif
  return EmptyAccountId();
}

// Move the window to the current user's desktop.
void MoveWindowToCurrentDesktop(aura::Window* window) {
#if defined(OS_CHROMEOS)
  if (!chrome::MultiUserWindowManager::GetInstance()->IsWindowOnDesktopOfUser(
          window, GetCurrentAccountId())) {
    chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
        window, GetCurrentAccountId());
  }
#endif
}

}  // namespace multi_user_util
