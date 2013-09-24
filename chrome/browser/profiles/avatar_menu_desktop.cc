// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"

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

  *image = cache.GetAvatarIconOfProfileAtIndex(index);
  *is_rectangle =
      cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
      cache.GetGAIAPictureOfProfileAtIndex(index);
}
