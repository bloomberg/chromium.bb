// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_TEST_UTILS_H_

#include <vector>

#include "ash/ash_constants.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class CommandLine;
class ScopedTempDir;
}  // namespace base

namespace chromeos {
namespace wallpaper_manager_test_utils {

// Colors used for different default wallpapers by CreateCmdlineWallpapers().
extern const SkColor kLargeDefaultWallpaperColor;
extern const SkColor kSmallDefaultWallpaperColor;
extern const SkColor kLargeGuestWallpaperColor;
extern const SkColor kSmallGuestWallpaperColor;

// A custom color, specifically chosen to not
// conflict with any of the default wallpaper colors.
extern const SkColor kCustomWallpaperColor;

// Dimension used for width and height of default wallpaper images. A
// small value is used to minimize the amount of time spent compressing
// and writing images.
extern const int kWallpaperSize;

// Creates compressed JPEG image of solid color.
// Result bytes are written to |output|.
// Returns true if gfx::JPEGCodec::Encode() succeeds.
bool CreateJPEGImage(int width,
                     int height,
                     SkColor color,
                     std::vector<unsigned char>* output);

// Creates a test image of given size.
gfx::ImageSkia CreateTestImage(int width, int height, SkColor color);

// Writes a JPEG image of the specified size and color to |path|. Returns
// true on success.
bool WriteJPEGFile(const base::FilePath& path,
                   int width,
                   int height,
                   SkColor color);

// Returns true if the color at the center of |image| is close to
// |expected_color|. (The center is used so small wallpaper images can be
// used.)
bool ImageIsNearColor(gfx::ImageSkia image, SkColor expected_color);

// Wait until all wallpaper loading is done, and WallpaperManager comes into
// a stable state.
void WaitAsyncWallpaperLoadFinished();

// Initializes default wallpaper paths "*default_*file" and writes JPEG
// wallpaper images to them.
// Only needs to be called (once) by tests that want to test loading of
// default wallpapers.
void CreateCmdlineWallpapers(const base::ScopedTempDir& dir,
                             scoped_ptr<base::CommandLine>* command_line);

}  // namespace wallpaper_manager_test_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_TEST_UTILS_H_
