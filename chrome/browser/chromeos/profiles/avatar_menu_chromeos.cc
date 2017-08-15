// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session_util.h"

#include "ui/gfx/image/image.h"

// static
AvatarMenu::ImageLoadStatus AvatarMenu::GetImageForMenuButton(
    const base::FilePath& profile_path,
    gfx::Image* image) {
  // ChromeOS avatar icon is circular.
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  *image = gfx::Image(GetAvatarImageForContext(profile));
  return AvatarMenu::ImageLoadStatus::LOADED;
}
