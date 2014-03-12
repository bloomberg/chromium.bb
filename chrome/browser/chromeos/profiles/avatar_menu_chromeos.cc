// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <string>

#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

// static
void AvatarMenu::GetImageForMenuButton(Profile* profile,
                                       gfx::Image* image,
                                       bool* is_rectangle) {
  static const gfx::ImageSkia* holder =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER);
  static const gfx::ImageSkia* holder_mask =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER_MASK);
  // ChromeOS avatar icon is circular.
  *is_rectangle = false;

  // Find the user for this profile.
  std::string user_id_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  chromeos::UserList users = chromeos::UserManager::Get()->GetLoggedInUsers();

  for (chromeos::UserList::const_iterator it = users.begin();
      it != users.end(); ++it) {
    if ((*it)->username_hash() == user_id_hash) {
      gfx::ImageSkia avatar = (*it)->image();
      gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
          avatar, skia::ImageOperations::RESIZE_BEST, holder->size());
      gfx::ImageSkia masked =
          gfx::ImageSkiaOperations::CreateMaskedImage(resized, *holder_mask);
      gfx::ImageSkia result =
          gfx::ImageSkiaOperations::CreateSuperimposedImage(*holder, masked);
      *image = gfx::Image(result);
      return;
    }
    LOG(FATAL) << "avatar image for the profile '"
               << profile->GetProfileName() << "' not found";
  }
}
