// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <string>

#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"

// static
void AvatarMenu::GetImageForMenuButton(Profile* profile,
                                       gfx::Image* image,
                                       bool* is_rectangle) {
  // Find the user for this profile.
  std::string user_id_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  chromeos::UserList users = chromeos::UserManager::Get()->GetLoggedInUsers();

  for (chromeos::UserList::const_iterator it = users.begin();
      it != users.end(); ++it) {
    if ((*it)->username_hash() == user_id_hash) {
      *image = gfx::Image((*it)->image());
      break;
    }
  }

  // ChromeOS user images are rectangular, unlike Chrome profile avatars.
  *is_rectangle = true;
}
