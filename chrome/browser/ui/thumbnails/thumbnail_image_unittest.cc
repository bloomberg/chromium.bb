// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace {
constexpr int kTestBitmapWidth = 200;
constexpr int kTestBitmapHeight = 123;
}  // namespace

class ThumbnailImageTest : public testing::Test {
 public:
  ThumbnailImageTest() = default;

 protected:
  static SkBitmap CreateBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseARGB(255, 0, 255, 0);
    return bitmap;
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  DISALLOW_COPY_AND_ASSIGN(ThumbnailImageTest);
};

TEST_F(ThumbnailImageTest, FromSkBitmap) {
  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  ThumbnailImage image = ThumbnailImage::FromSkBitmap(bitmap);
  EXPECT_TRUE(image.HasData());
  EXPECT_GE(image.GetStorageSize(), 0U);
  EXPECT_LE(image.GetStorageSize(), bitmap.computeByteSize());
  gfx::ImageSkia image_skia = image.AsImageSkia();
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight), image_skia.size());
}

TEST_F(ThumbnailImageTest, FromSkBitmapAsync) {
  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  ThumbnailImage image;

  base::RunLoop run_loop;
  ThumbnailImage::FromSkBitmapAsync(
      bitmap, base::BindOnce(
                  [](base::OnceClosure end_closure, ThumbnailImage* dest_image,
                     ThumbnailImage source_image) {
                    *dest_image = source_image;
                    std::move(end_closure).Run();
                  },
                  run_loop.QuitClosure(), base::Unretained(&image)));
  run_loop.Run();

  EXPECT_TRUE(image.HasData());
  EXPECT_GE(image.GetStorageSize(), 0U);
  EXPECT_LE(image.GetStorageSize(), bitmap.computeByteSize());
  gfx::ImageSkia image_skia = image.AsImageSkia();
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight), image_skia.size());
}

TEST_F(ThumbnailImageTest, AsImageSkia) {
  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  ThumbnailImage image = ThumbnailImage::FromSkBitmap(bitmap);
  EXPECT_TRUE(image.HasData());

  gfx::ImageSkia image_skia = image.AsImageSkia();
  EXPECT_TRUE(image_skia.IsThreadSafe());
  EXPECT_FALSE(image_skia.isNull());
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight), image_skia.size());
}

TEST_F(ThumbnailImageTest, AsImageSkiaAsync) {
  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  ThumbnailImage image = ThumbnailImage::FromSkBitmap(bitmap);
  EXPECT_TRUE(image.HasData());

  base::RunLoop run_loop;
  gfx::ImageSkia image_skia;
  const bool can_convert = image.AsImageSkiaAsync(base::BindOnce(
      [](base::OnceClosure end_closure, gfx::ImageSkia* dest_image,
         gfx::ImageSkia source_image) {
        *dest_image = source_image;
        std::move(end_closure).Run();
      },
      run_loop.QuitClosure(), base::Unretained(&image_skia)));
  ASSERT_TRUE(can_convert);
  run_loop.Run();

  EXPECT_TRUE(image_skia.IsThreadSafe());
  EXPECT_FALSE(image_skia.isNull());
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight), image_skia.size());
}
