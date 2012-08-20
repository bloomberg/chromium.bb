// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_image.h"

#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::IconImage;

namespace {

class ExtensionIconImageTest : public testing::Test,
                               public IconImage::Observer {
 public:
  ExtensionIconImageTest()
      : image_loaded_count_(0),
        image_load_failure_count_(0),
        quit_in_image_loaded_(false),
        ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {
  }

  virtual ~ExtensionIconImageTest() {}

  void WaitForImageLoad() {
    // ExtensionIconImage may return synchronously, in which case there's
    // nothing to wait for.
    if (image_loaded_count_ > 0 || image_load_failure_count_ > 0)
      return;
    quit_in_image_loaded_ = true;
    MessageLoop::current()->Run();
    quit_in_image_loaded_ = false;
  }

  int ImageLoadedCount() {
    int result = image_loaded_count_;
    image_loaded_count_ = 0;
    return result;
  }

  int ImageLoadFailureCount() {
    int result = image_load_failure_count_;
    image_load_failure_count_ = 0;
    return result;
  }

  scoped_refptr<Extension> CreateExtension(const char* name,
                                           Extension::Location location) {
    // Create and load an extension.
    FilePath test_file;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_file)) {
      EXPECT_FALSE(true);
      return NULL;
    }
    test_file = test_file.AppendASCII("extensions").AppendASCII(name);
    int error_code = 0;
    std::string error;
    JSONFileValueSerializer serializer(test_file.AppendASCII("app.json"));
    scoped_ptr<DictionaryValue> valid_value(
        static_cast<DictionaryValue*>(serializer.Deserialize(&error_code,
                                                             &error)));
    EXPECT_EQ(0, error_code) << error;
    if (error_code != 0)
      return NULL;

    EXPECT_TRUE(valid_value.get());
    if (!valid_value.get())
      return NULL;

    return Extension::Create(test_file, location, *valid_value,
                             Extension::NO_FLAGS, &error);
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    file_thread_.Start();
    io_thread_.Start();
  }

  // IconImage::Delegate overrides:
  virtual void OnExtensionIconImageChanged(IconImage* image) OVERRIDE {
    image_loaded_count_++;
    if (quit_in_image_loaded_)
      MessageLoop::current()->Quit();
  }

  virtual void OnIconImageLoadFailed(IconImage* image,
                                     ui::ScaleFactor scale_factor) OVERRIDE {
    image_load_failure_count_++;
    if (quit_in_image_loaded_)
      MessageLoop::current()->Quit();
  }

 private:
  int image_loaded_count_;
  int image_load_failure_count_;
  bool quit_in_image_loaded_;
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconImageTest);
};

}  // namespace

TEST_F(ExtensionIconImageTest, Basic) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  IconImage image(
      extension,
      extension->icons(),
      extension_misc::EXTENSION_ICON_BITTY,
      this);

  // No representations in |image_| yet.
  gfx::ImageSkia::ImageSkiaReps image_reps = image.image_skia().image_reps();
  ASSERT_EQ(0u, image_reps.size());

  // Gets representation for a scale factor.
  image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  EXPECT_EQ(0, ImageLoadFailureCount());

  // Gets representation for an additional scale factor.
  image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);
  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  EXPECT_EQ(0, ImageLoadFailureCount());

  gfx::ImageSkiaRep image_rep =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_EQ(extension_misc::EXTENSION_ICON_BITTY,
            image_rep.pixel_width());

  image_rep = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_EQ(extension_misc::EXTENSION_ICON_SMALL,
            image_rep.pixel_width());
}

// If we can't load icon with the exact size, but a bigger resource is
// available.
TEST_F(ExtensionIconImageTest, FallbackToBigger) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  IconImage image(
      extension,
      extension->icons(),
      extension_misc::EXTENSION_ICON_BITTY,
      this);

  // Get representation for 2x.
  image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  EXPECT_EQ(0, ImageLoadFailureCount());

  gfx::ImageSkiaRep image_rep =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  // We should have found a bigger resource and it should have been resized.
  EXPECT_EQ(ui::SCALE_FACTOR_200P, image_rep.scale_factor());
  EXPECT_EQ(2 * extension_misc::EXTENSION_ICON_BITTY,
            image_rep.pixel_width());
}

// There is no resource with either exact or bigger size, but there is a smaller
// resource.
TEST_F(ExtensionIconImageTest, FallbackToSmallerWhenNoBigger) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  IconImage image(
      extension,
      extension->icons(),
      extension_misc::EXTENSION_ICON_SMALL,
      this);

  // Attempt to get representation for 2x.
  image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  EXPECT_EQ(0, ImageLoadFailureCount());

  gfx::ImageSkiaRep image_rep =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  // We should have loaded the biggest smaller resource. In this case the
  // loaded resource should not be resized.
  EXPECT_EQ(ui::SCALE_FACTOR_200P, image_rep.scale_factor());
  EXPECT_EQ(extension_misc::EXTENSION_ICON_MEDIUM,
            image_rep.pixel_width());
}

// There is no resource with exact size, but there is a smaller and a bigger
// one. Requested size is smaller than 32 though, so the smaller resource should
// be loaded.
TEST_F(ExtensionIconImageTest, FallbackToSmaller) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  IconImage image(
      extension,
      extension->icons(),
      17,
      this);

  // Attempt to get representation for 1x.
  image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  EXPECT_EQ(0, ImageLoadFailureCount());

  gfx::ImageSkiaRep image_rep =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  // We should have loaded smaller (not resized) resource.
  EXPECT_EQ(ui::SCALE_FACTOR_100P, image_rep.scale_factor());
  EXPECT_EQ(extension_misc::EXTENSION_ICON_BITTY,
            image_rep.pixel_width());
}

// If resource set is empty, failure should be reported.
TEST_F(ExtensionIconImageTest, NoResources) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionIconSet empty_icon_set;

  IconImage image(
      extension,
      empty_icon_set,
      extension_misc::EXTENSION_ICON_SMALLISH,
      this);

  // Attempt to get representation for 2x.
  image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  WaitForImageLoad();
  EXPECT_EQ(0, ImageLoadedCount());
  EXPECT_EQ(1, ImageLoadFailureCount());
}
