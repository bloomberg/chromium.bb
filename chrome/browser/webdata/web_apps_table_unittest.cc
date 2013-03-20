// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::Time;

class WebAppsTableTest : public testing::Test {
 public:
  WebAppsTableTest() {}
  virtual ~WebAppsTableTest() {}

 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.path().AppendASCII("TestWebDatabase");

    table_.reset(new WebAppsTable);
    db_.reset(new WebDatabase);
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_, std::string()));
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<WebAppsTable> table_;
  scoped_ptr<WebDatabase> db_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppsTableTest);
};


TEST_F(WebAppsTableTest, WebAppHasAllImages) {
  GURL url("http://google.com/");

  // Initial value for unknown web app should be false.
  EXPECT_FALSE(table_->GetWebAppHasAllImages(url));

  // Set the value and make sure it took.
  EXPECT_TRUE(table_->SetWebAppHasAllImages(url, true));
  EXPECT_TRUE(table_->GetWebAppHasAllImages(url));

  // Remove the app and make sure value reverts to default.
  EXPECT_TRUE(table_->RemoveWebApp(url));
  EXPECT_FALSE(table_->GetWebAppHasAllImages(url));
}

TEST_F(WebAppsTableTest, WebAppImages) {
  GURL url("http://google.com/");

  // Web app should initially have no images.
  std::vector<SkBitmap> images;
  ASSERT_TRUE(table_->GetWebAppImages(url, &images));
  ASSERT_EQ(0U, images.size());

  // Add an image.
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  ASSERT_TRUE(table_->SetWebAppImage(url, image));

  // Make sure we get the image back.
  ASSERT_TRUE(table_->GetWebAppImages(url, &images));
  ASSERT_EQ(1U, images.size());
  ASSERT_EQ(16, images[0].width());
  ASSERT_EQ(16, images[0].height());

  // Add another 16x16 image and make sure it replaces the original.
  image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);

  // Set some random pixels so that we can identify the image. We don't use
  // transparent images because of pre-multiplication rounding errors.
  SkColor test_pixel_1 = 0xffccbbaa;
  SkColor test_pixel_2 = 0xffaabbaa;
  SkColor test_pixel_3 = 0xff339966;
  image.getAddr32(0, 1)[0] = test_pixel_1;
  image.getAddr32(0, 1)[1] = test_pixel_2;
  image.getAddr32(0, 1)[2] = test_pixel_3;

  ASSERT_TRUE(table_->SetWebAppImage(url, image));
  images.clear();
  ASSERT_TRUE(table_->GetWebAppImages(url, &images));
  ASSERT_EQ(1U, images.size());
  ASSERT_EQ(16, images[0].width());
  ASSERT_EQ(16, images[0].height());
  images[0].lockPixels();
  ASSERT_TRUE(images[0].getPixels() != NULL);
  ASSERT_EQ(test_pixel_1, images[0].getAddr32(0, 1)[0]);
  ASSERT_EQ(test_pixel_2, images[0].getAddr32(0, 1)[1]);
  ASSERT_EQ(test_pixel_3, images[0].getAddr32(0, 1)[2]);
  images[0].unlockPixels();

  // Add another image at a bigger size.
  image.setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  ASSERT_TRUE(table_->SetWebAppImage(url, image));

  // Make sure we get both images back.
  images.clear();
  ASSERT_TRUE(table_->GetWebAppImages(url, &images));
  ASSERT_EQ(2U, images.size());
  if (images[0].width() == 16) {
    ASSERT_EQ(16, images[0].width());
    ASSERT_EQ(16, images[0].height());
    ASSERT_EQ(32, images[1].width());
    ASSERT_EQ(32, images[1].height());
  } else {
    ASSERT_EQ(32, images[0].width());
    ASSERT_EQ(32, images[0].height());
    ASSERT_EQ(16, images[1].width());
    ASSERT_EQ(16, images[1].height());
  }
}
