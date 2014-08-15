// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"

#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "components/user_manager/user_manager.h"
#endif

namespace multi_user_util {

std::string GetUserIDFromProfile(Profile* profile) {
  return GetUserIDFromEmail(profile->GetOriginalProfile()->GetProfileName());
}

std::string GetUserIDFromEmail(const std::string& email) {
  // |email| and profile name could be empty if not yet logged in or guest mode.
  return email.empty() ?
      email : gaia::CanonicalizeEmail(gaia::SanitizeEmail(email));
}

Profile* GetProfileFromUserID(const std::string& user_id) {
  // Unit tests can end up here without a |g_browser_process|.
  if (!g_browser_process || !g_browser_process->profile_manager())
    return NULL;

  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();

  std::vector<Profile*>::const_iterator profile_iterator = profiles.begin();
  for (; profile_iterator != profiles.end(); ++profile_iterator) {
    if (GetUserIDFromProfile(*profile_iterator) == user_id)
      return *profile_iterator;
  }
  return NULL;
}

Profile* GetProfileFromWindow(aura::Window* window) {
#if defined(OS_CHROMEOS)
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();
  // We might come here before the manager got created - or in a unit test.
  if (!manager)
    return NULL;
  const std::string user_id = manager->GetUserPresentingWindow(window);
  return user_id.empty() ? NULL :
                           multi_user_util::GetProfileFromUserID(user_id);
#else
  return NULL;
#endif
}

bool IsProfileFromActiveUser(Profile* profile) {
#if defined(OS_CHROMEOS)
  return GetUserIDFromProfile(profile) ==
         user_manager::UserManager::Get()->GetActiveUser()->email();
#else
  // In non Chrome OS configurations this will be always true since this only
  // makes sense in separate desktop mode.
  return true;
#endif
}

const std::string& GetCurrentUserId() {
#if defined(OS_CHROMEOS)
  return user_manager::UserManager::Get()->GetActiveUser()->email();
#else
  return base::EmptyString();
#endif
}

// Move the window to the current user's desktop.
void MoveWindowToCurrentDesktop(aura::Window* window) {
#if defined(OS_CHROMEOS)
  chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      window,
      GetCurrentUserId());
#endif
}

}  // namespace multi_user_util
