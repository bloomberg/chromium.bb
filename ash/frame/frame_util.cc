// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/frame_util.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "components/user_manager/user_info.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

gfx::Image GetAvatarImageForContext(content::BrowserContext* context) {
  static const gfx::ImageSkia* holder =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER);
  static const gfx::ImageSkia* holder_mask =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER_MASK);
  gfx::ImageSkia user_image = Shell::GetInstance()
                                  ->session_state_delegate()
                                  ->GetUserInfo(context)
                                  ->GetImage();
  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      user_image, skia::ImageOperations::RESIZE_BEST, holder->size());
  gfx::ImageSkia masked =
      gfx::ImageSkiaOperations::CreateMaskedImage(resized, *holder_mask);
  gfx::ImageSkia result =
      gfx::ImageSkiaOperations::CreateSuperimposedImage(*holder, masked);
  return gfx::Image(result);
}

}  // namespace ash

