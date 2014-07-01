// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager_test_utils.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace chromeos {

namespace {

class TestWallpaperObserverPendingListEmpty
    : public WallpaperManager::Observer {
 public:
  explicit TestWallpaperObserverPendingListEmpty(
      WallpaperManager* wallpaper_manager)
      : empty_(false), wallpaper_manager_(wallpaper_manager) {
    DCHECK(wallpaper_manager_);
    wallpaper_manager_->AddObserver(this);
  }

  virtual ~TestWallpaperObserverPendingListEmpty() {
    wallpaper_manager_->RemoveObserver(this);
  }

  virtual void OnWallpaperAnimationFinished(
      const std::string& user_id) OVERRIDE {}

  virtual void OnPendingListEmptyForTesting() OVERRIDE {
    empty_ = true;
    base::MessageLoop::current()->Quit();
  }

  void WaitForPendingListEmpty() {
    if (wallpaper_manager_->GetPendingListSizeForTesting() == 0) {
      empty_ = true;
      return;
    }
    while (!empty_)
      base::RunLoop().Run();
  }

 private:
  bool empty_;
  WallpaperManager* wallpaper_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperObserverPendingListEmpty);
};

}  // namespace

namespace wallpaper_manager_test_utils {

const SkColor kLargeDefaultWallpaperColor = SK_ColorRED;
const SkColor kSmallDefaultWallpaperColor = SK_ColorGREEN;
const SkColor kLargeGuestWallpaperColor = SK_ColorBLUE;
const SkColor kSmallGuestWallpaperColor = SK_ColorYELLOW;

const SkColor kCustomWallpaperColor = SK_ColorMAGENTA;

const int kWallpaperSize = 2;

bool CreateJPEGImage(int width,
                     int height,
                     SkColor color,
                     std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);

  const int kQuality = 80;
  if (!gfx::JPEGCodec::Encode(
          static_cast<const unsigned char*>(bitmap.getPixels()),
          gfx::JPEGCodec::FORMAT_SkBitmap,
          width,
          height,
          bitmap.rowBytes(),
          kQuality,
          output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }
  return true;
}

gfx::ImageSkia CreateTestImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

bool WriteJPEGFile(const base::FilePath& path,
                   int width,
                   int height,
                   SkColor color) {
  std::vector<unsigned char> output;
  if (!CreateJPEGImage(width, height, color, &output))
    return false;

  size_t bytes_written = base::WriteFile(
      path, reinterpret_cast<const char*>(&output[0]), output.size());
  if (bytes_written != output.size()) {
    LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
               << output.size() << " to " << path.value();
    return false;
  }
  return true;
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

  bitmap->lockPixels();
  gfx::Point center = gfx::Rect(image.size()).CenterPoint();
  SkColor image_color = bitmap->getColor(center.x(), center.y());
  bitmap->unlockPixels();

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

void WaitAsyncWallpaperLoadFinished() {
  TestWallpaperObserverPendingListEmpty observer(WallpaperManager::Get());
  observer.WaitForPendingListEmpty();
}

void CreateCmdlineWallpapers(const base::ScopedTempDir& dir,
                             scoped_ptr<base::CommandLine>* command_line) {
  std::vector<std::string> options;
  options.push_back(std::string("WM_Test_cmdline"));
  const base::FilePath small_file =
      dir.path().Append(FILE_PATH_LITERAL("small.jpg"));
  options.push_back(std::string("--") +
                    ash::switches::kAshDefaultWallpaperSmall + "=" +
                    small_file.value());
  const base::FilePath large_file =
      dir.path().Append(FILE_PATH_LITERAL("large.jpg"));
  options.push_back(std::string("--") +
                    ash::switches::kAshDefaultWallpaperLarge + "=" +
                    large_file.value());

  const base::FilePath guest_small_file =
      dir.path().Append(FILE_PATH_LITERAL("guest_small.jpg"));
  options.push_back(std::string("--") + ash::switches::kAshGuestWallpaperSmall +
                    "=" + guest_small_file.value());
  const base::FilePath guest_large_file =
      dir.path().Append(FILE_PATH_LITERAL("guest_large.jpg"));
  options.push_back(std::string("--") + ash::switches::kAshGuestWallpaperLarge +
                    "=" + guest_large_file.value());

  ASSERT_TRUE(WriteJPEGFile(
      small_file, kWallpaperSize, kWallpaperSize, kSmallDefaultWallpaperColor));
  ASSERT_TRUE(WriteJPEGFile(
      large_file, kWallpaperSize, kWallpaperSize, kLargeDefaultWallpaperColor));

  ASSERT_TRUE(WriteJPEGFile(guest_small_file,
                            kWallpaperSize,
                            kWallpaperSize,
                            kSmallGuestWallpaperColor));
  ASSERT_TRUE(WriteJPEGFile(guest_large_file,
                            kWallpaperSize,
                            kWallpaperSize,
                            kLargeGuestWallpaperColor));

  command_line->reset(new base::CommandLine(options));
  WallpaperManager::Get()->SetCommandLineForTesting(command_line->get());
}

}  // namespace wallpaper_manager_test_utils

}  // namespace chromeos
