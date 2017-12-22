// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_TEST_UTILS_H_

#include <vector>

#include "ash/ash_constants.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace wallpaper_manager_test_utils {

// Custom wallpaper colors.
extern const SkColor kLargeCustomWallpaperColor;
extern const SkColor kSmallCustomWallpaperColor;

// Dimension used for width and height of default wallpaper images. A
// small value is used to minimize the amount of time spent compressing
// and writing images.
extern const int kWallpaperSize;

// Creates a test image of given size.
gfx::ImageSkia CreateTestImage(int width, int height, SkColor color);

// Returns true if the color at the center of |image| is close to
// |expected_color|. (The center is used so small wallpaper images can be
// used.)
bool ImageIsNearColor(gfx::ImageSkia image, SkColor expected_color);

}  // namespace wallpaper_manager_test_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_TEST_UTILS_H_
