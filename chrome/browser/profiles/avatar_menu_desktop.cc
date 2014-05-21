// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/base/resource/resource_bundle.h"

// static
void AvatarMenu::GetImageForMenuButton(Profile* profile,
                                       gfx::Image* image,
                                       bool* is_rectangle) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (index == std::string::npos) {
    NOTREACHED();
    return;
  }

  // Ensure we are using the default resource, not the downloaded high-res one.
  const size_t icon_index = cache.GetAvatarIconIndexOfProfileAtIndex(index);
  const int resource_id =
      profiles::GetDefaultAvatarIconResourceIDAtIndex(icon_index);
  *image = ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  *is_rectangle =
      cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
      cache.GetGAIAPictureOfProfileAtIndex(index);
}
