// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/wallpaper_resizer.h"

#include "ash/desktop_background/wallpaper_resizer_observer.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/image/image_skia_rep.h"

using aura::RootWindow;
using aura::Window;

namespace {

const int kTestImageWidth = 5;
const int kTestImageHeight = 2;
const int kTargetWidth = 1;
const int kTargetHeight = 1;
const uint32_t kExpectedCenter = 0x02020202u;
const uint32_t kExpectedCenterCropped = 0x03030303u;
const uint32_t kExpectedStretch = 0x04040404u;
const uint32_t kExpectedTile = 0;

gfx::ImageSkia CreateTestImage(const gfx::Size& size) {
  SkBitmap src;
  int w = size.width();
  int h = size.height();
  src.setConfig(SkBitmap::kARGB_8888_Config, w, h);
  src.allocPixels();

  // Fill bitmap with data.
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t component = static_cast<uint8_t>(y * w + x);
      const SkColor pixel = SkColorSetARGB(component, component,
                                           component, component);
      *(src.getAddr32(x, y)) = pixel;
    }
  }

  return gfx::ImageSkia::CreateFrom1xBitmap(src);
}

bool IsColor(const gfx::ImageSkia& image, const uint32_t expect) {
  EXPECT_EQ(image.width(), kTargetWidth);
  EXPECT_EQ(image.height(), kTargetHeight);
  const SkBitmap* image_bitmap = image.bitmap();
  SkAutoLockPixels image_lock(*image_bitmap);
  return *image_bitmap->getAddr32(0, 0) == expect;
}

}  // namespace

namespace ash {
namespace internal {

class WallpaperResizerTest : public test::AshTestBase,
                             public WallpaperResizerObserver {
 public:
  WallpaperResizerTest() {}
  virtual ~WallpaperResizerTest() {}

  gfx::ImageSkia Resize(const WallpaperInfo& info,
                        const gfx::Size& target_size,
                        const gfx::ImageSkia& image) {
    scoped_ptr<WallpaperResizer> resizer;
    resizer.reset(new WallpaperResizer(info, target_size, image));
    resizer->AddObserver(this);
    resizer->StartResize();
    WaitForResize();
    resizer->RemoveObserver(this);
    return resizer->wallpaper_image();
  }

  void WaitForResize() {
    base::MessageLoop::current()->Run();
  }

  virtual void OnWallpaperResized() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WallpaperResizerTest);
};

TEST_F(WallpaperResizerTest, BasicResize) {
  // Keeps in sync with WallpaperLayout enum.
  WallpaperLayout layouts[4] = {
      WALLPAPER_LAYOUT_CENTER,
      WALLPAPER_LAYOUT_CENTER_CROPPED,
      WALLPAPER_LAYOUT_STRETCH,
      WALLPAPER_LAYOUT_TILE,
  };
  const int length = arraysize(layouts);

  for (int i = 0; i < length; i++) {
    WallpaperLayout layout = layouts[i];
    WallpaperInfo info = { 0, layout };
    gfx::ImageSkia small_image(gfx::ImageSkiaRep(gfx::Size(10, 20),
                                                 ui::SCALE_FACTOR_100P));

    gfx::ImageSkia resized_small = Resize(info, gfx::Size(800, 600),
                                          small_image);
    EXPECT_EQ(10, resized_small.width());
    EXPECT_EQ(20, resized_small.height());

    gfx::ImageSkia large_image(gfx::ImageSkiaRep(gfx::Size(1000, 1000),
                                                 ui::SCALE_FACTOR_100P));
    gfx::ImageSkia resized_large = Resize(info, gfx::Size(800, 600),
                                          large_image);
    EXPECT_EQ(800, resized_large.width());
    EXPECT_EQ(600, resized_large.height());
  }
}

// Test for crbug.com/244629. "CENTER_CROPPED generates the same image as
// STRETCH layout"
TEST_F(WallpaperResizerTest, AllLayoutDifferent) {
  gfx::ImageSkia image = CreateTestImage(
      gfx::Size(kTestImageWidth, kTestImageHeight));

  gfx::Size target_size = gfx::Size(kTargetWidth, kTargetHeight);
  WallpaperInfo info_center = { 0, WALLPAPER_LAYOUT_CENTER };
  gfx::ImageSkia center = Resize(info_center, target_size, image);

  WallpaperInfo info_center_cropped = { 0, WALLPAPER_LAYOUT_CENTER_CROPPED };
  gfx::ImageSkia center_cropped = Resize(info_center_cropped, target_size,
                                         image);

  WallpaperInfo info_stretch = { 0, WALLPAPER_LAYOUT_STRETCH };
  gfx::ImageSkia stretch = Resize(info_stretch, target_size, image);

  WallpaperInfo info_tile = { 0, WALLPAPER_LAYOUT_TILE };
  gfx::ImageSkia tile = Resize(info_tile, target_size, image);

  EXPECT_TRUE(IsColor(center, kExpectedCenter));
  EXPECT_TRUE(IsColor(center_cropped, kExpectedCenterCropped));
  EXPECT_TRUE(IsColor(stretch, kExpectedStretch));
  EXPECT_TRUE(IsColor(tile, kExpectedTile));
}

}  // namespace internal
}  // namespace ash
