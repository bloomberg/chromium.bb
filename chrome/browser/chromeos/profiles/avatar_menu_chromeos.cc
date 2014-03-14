// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <string>

#include "ash/frame/frame_util.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/image/image.h"

// static
void AvatarMenu::GetImageForMenuButton(Profile* profile,
                                       gfx::Image* image,
                                       bool* is_rectangle) {
  // ChromeOS avatar icon is circular.
  *is_rectangle = false;
  *image = ash::GetAvatarImageForContext(profile);
}
