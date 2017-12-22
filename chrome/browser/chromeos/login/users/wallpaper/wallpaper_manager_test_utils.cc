// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h"

#include <stddef.h>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {
namespace wallpaper_manager_test_utils {

const SkColor kLargeCustomWallpaperColor = SK_ColorDKGRAY;
const SkColor kSmallCustomWallpaperColor = SK_ColorLTGRAY;

const int kWallpaperSize = 2;

gfx::ImageSkia CreateTestImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

bool ImageIsNearColor(gfx::ImageSkia image, SkColor expected_color) {
  if (image.size().IsEmpty()) {
    LOG(ERROR) << "Image is empty";
    return false;
  }

  const SkBitmap* bitmap = image.bitmap();
  if (!bitmap) {
    LOG(ERROR) << "Unable to get bitmap from image";
    return false;
  }

  gfx::Point center = gfx::Rect(image.size()).CenterPoint();
  SkColor image_color = bitmap->getColor(center.x(), center.y());

  const int kDiff = 3;
  if (std::abs(static_cast<int>(SkColorGetA(image_color)) -
               static_cast<int>(SkColorGetA(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetR(image_color)) -
               static_cast<int>(SkColorGetR(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetG(image_color)) -
               static_cast<int>(SkColorGetG(expected_color))) > kDiff ||
      std::abs(static_cast<int>(SkColorGetB(image_color)) -
               static_cast<int>(SkColorGetB(expected_color))) > kDiff) {
    LOG(ERROR) << "Expected color near 0x" << std::hex << expected_color
               << " but got 0x" << image_color;
    return false;
  }

  return true;
}

}  // namespace wallpaper_manager_test_utils

}  // namespace chromeos
