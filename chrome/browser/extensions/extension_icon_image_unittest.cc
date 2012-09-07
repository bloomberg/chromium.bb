// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_image.h"

#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::IconImage;

namespace {

SkBitmap CreateBlankBitmapForScale(int size_dip, ui::ScaleFactor scale_factor) {
  SkBitmap bitmap;
  const float scale = ui::GetScaleFactorScale(scale_factor);
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   static_cast<int>(size_dip * scale),
                   static_cast<int>(size_dip * scale));
  bitmap.allocPixels();
  bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));
  return bitmap;
}

// Used to test behaviour including images defined by an image skia source.
// |GetImageForScale| simply returns image representation from the image given
// in the ctor.
class MockImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  explicit MockImageSkiaSource(const gfx::ImageSkia& image)
      : image_(image) {
  }
  virtual ~MockImageSkiaSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    return image_.GetRepresentation(scale_factor);
  }

 private:
  gfx::ImageSkia image_;
};

// Helper class for synchronously loading extension image resource.
class TestImageLoader : public ImageLoadingTracker::Observer {
 public:
  explicit TestImageLoader(const Extension* extension)
      : extension_(extension),
        waiting_(false),
        image_loaded_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
  }
  virtual ~TestImageLoader() {}

  // ImageLoadingTracker::Observer override.
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE {
    image_ = image;
    image_loaded_ = true;
    if (waiting_)
      MessageLoop::current()->Quit();
  }

  SkBitmap LoadBitmap(const std::string& path,
                      int size,
                      ImageLoadingTracker::CacheParam cache_param) {
    image_loaded_ = false;

    tracker_.LoadImage(extension_,
                       extension_->GetResource(path),
                       gfx::Size(size, size),
                       cache_param);

    // If |image_| still hasn't been loaded (i.e. it is being loaded
    // asynchronously), wait for it.
    if (!image_loaded_) {
      waiting_ = true;
      MessageLoop::current()->Run();
      waiting_ = false;
    }

    EXPECT_TRUE(image_loaded_);

    return image_.IsEmpty() ? SkBitmap() : *image_.ToSkBitmap();
  }

 private:
  const Extension* extension_;
  bool waiting_;
  bool image_loaded_;
  gfx::Image image_;
  ImageLoadingTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestImageLoader);
};

class ExtensionIconImageTest : public testing::Test,
                               public IconImage::Observer {
 public:
  ExtensionIconImageTest()
      : image_loaded_count_(0),
        quit_in_image_loaded_(false),
        ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {
  }

  virtual ~ExtensionIconImageTest() {}

  void WaitForImageLoad() {
    quit_in_image_loaded_ = true;
    MessageLoop::current()->Run();
    quit_in_image_loaded_ = false;
  }

  int ImageLoadedCount() {
    int result = image_loaded_count_;
    image_loaded_count_ = 0;
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

  gfx::ImageSkia GetDefaultIcon() {
    return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_EXTENSIONS_FAVICON);
  }

  // Loads an image to be used in test from the extension.
  // The image will be loaded from the relative path |path|.
  SkBitmap GetTestBitmap(const Extension* extension,
                         const std::string& path,
                         int size,
                         ImageLoadingTracker::CacheParam cache_param) {
    TestImageLoader image_loader(extension);
    return image_loader.LoadBitmap(path, size, cache_param);
  }

 private:
  int image_loaded_count_;
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

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 =
      GetTestBitmap(extension, "16.png", 16, ImageLoadingTracker::DONT_CACHE);
  ASSERT_FALSE(bitmap_16.empty());

  // There is no image of size 32 defined in the extension manifest, so we
  // should expect manifest image of size 48 resized to size 32.
  SkBitmap bitmap_48_resized_to_32 =
      GetTestBitmap(extension, "48.png", 32, ImageLoadingTracker::DONT_CACHE);
  ASSERT_FALSE(bitmap_48_resized_to_32.empty());

  IconImage image(extension, extension->icons(), 16, default_icon, this);

  // No representations in |image_| yet.
  gfx::ImageSkia::ImageSkiaReps image_reps = image.image_skia().image_reps();
  ASSERT_EQ(0u, image_reps.size());

  // Gets representation for a scale factor.
  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  // Before the image representation is loaded, image should contain blank
  // image representation.
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(16, ui::SCALE_FACTOR_100P)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  // We should get the right representation now.
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_16));
  EXPECT_EQ(16, representation.pixel_width());

  // Gets representation for an additional scale factor.
  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(16, ui::SCALE_FACTOR_200P)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(2u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  // Image should have been resized.
  EXPECT_EQ(32, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(),
                                   bitmap_48_resized_to_32));
}

// There is no resource with either exact or bigger size, but there is a smaller
// resource.
TEST_F(ExtensionIconImageTest, FallbackToSmallerWhenNoBigger) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_48 =
      GetTestBitmap(extension, "48.png", 48, ImageLoadingTracker::DONT_CACHE);
  ASSERT_FALSE(bitmap_48.empty());

  IconImage image(extension, extension->icons(), 32, default_icon, this);

  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_200P);

  // We should have loaded the biggest smaller resource. In this case the
  // loaded resource should not be resized.
  EXPECT_EQ(ui::SCALE_FACTOR_200P, representation.scale_factor());
  EXPECT_EQ(48, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_48));
}

// There is no resource with exact size, but there is a smaller and a bigger
// one. Requested size is smaller than 32 though, so the smaller resource should
// be loaded.
TEST_F(ExtensionIconImageTest, FallbackToSmaller) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 =
      GetTestBitmap(extension, "16.png", 16, ImageLoadingTracker::DONT_CACHE);
  ASSERT_FALSE(bitmap_16.empty());

  IconImage image(extension, extension->icons(), 17, default_icon, this);

  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  // We should have loaded smaller (not resized) resource.
  EXPECT_EQ(ui::SCALE_FACTOR_100P, representation.scale_factor());
  EXPECT_EQ(16, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_16));
}

// If resource set is empty, |GetRepresentation| should synchronously return
// default icon, without notifying observer of image change.
TEST_F(ExtensionIconImageTest, NoResources) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionIconSet empty_icon_set;
  gfx::ImageSkia default_icon = GetDefaultIcon();

  IconImage image(extension, empty_icon_set, 24, default_icon, this);

  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      default_icon.GetRepresentation(ui::SCALE_FACTOR_100P).sk_bitmap()));

  EXPECT_EQ(0, ImageLoadedCount());
  // We should have a default icon representation.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      default_icon.GetRepresentation(ui::SCALE_FACTOR_100P).sk_bitmap()));
}

// If resource set is invalid, image load should be done asynchronously and
// the observer should be notified when it's done. |GetRepresentation| should
// return the default icon representation once image load is done.
TEST_F(ExtensionIconImageTest, InvalidResource) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionIconSet invalid_icon_set;
  invalid_icon_set.Add(24, "invalid.png");

  gfx::ImageSkia default_icon = GetDefaultIcon();

  IconImage image(extension, invalid_icon_set, 24, default_icon, this);

  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(24, ui::SCALE_FACTOR_100P)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  // We should have default icon representation now.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      default_icon.GetRepresentation(ui::SCALE_FACTOR_100P).sk_bitmap()));
}

// Test that IconImage works with lazily (but synchronously) created default
// icon when IconImage returns synchronously.
TEST_F(ExtensionIconImageTest, LazyDefaultIcon) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();
  gfx::ImageSkia lazy_default_icon(new MockImageSkiaSource(default_icon),
                                    default_icon.size());

  ExtensionIconSet empty_icon_set;

  IconImage image(extension, empty_icon_set, 128, lazy_default_icon, this);

  ASSERT_FALSE(lazy_default_icon.HasRepresentation(ui::SCALE_FACTOR_100P));

  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  // The resouce set is empty, so we should get the result right away.
  EXPECT_TRUE(lazy_default_icon.HasRepresentation(ui::SCALE_FACTOR_100P));
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      default_icon.GetRepresentation(ui::SCALE_FACTOR_100P).sk_bitmap()));

  // We should have a default icon representation.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());
}

// Test that IconImage works with lazily (but synchronously) created default
// icon when IconImage returns asynchronously.
TEST_F(ExtensionIconImageTest, LazyDefaultIcon_AsyncIconImage) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();
  gfx::ImageSkia lazy_default_icon(new MockImageSkiaSource(default_icon),
                                    default_icon.size());

  ExtensionIconSet invalid_icon_set;
  invalid_icon_set.Add(24, "invalid.png");

  IconImage image(extension, invalid_icon_set, 24, lazy_default_icon, this);

  ASSERT_FALSE(lazy_default_icon.HasRepresentation(ui::SCALE_FACTOR_100P));

  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  // We should have default icon representation now.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  EXPECT_TRUE(lazy_default_icon.HasRepresentation(ui::SCALE_FACTOR_100P));

  representation = image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      default_icon.GetRepresentation(ui::SCALE_FACTOR_100P).sk_bitmap()));
}

TEST_F(ExtensionIconImageTest, LoadPrecachedImage) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Note the cache parameter.
  SkBitmap bitmap_16 =
      GetTestBitmap(extension, "16.png", 16, ImageLoadingTracker::CACHE);
  ASSERT_FALSE(bitmap_16.empty());

  IconImage image(extension, extension->icons(), 16, default_icon, this);

  // No representations in |image_| yet.
  gfx::ImageSkia::ImageSkiaReps image_reps = image.image_skia().image_reps();
  ASSERT_EQ(0u, image_reps.size());

  // Gets representation for a scale factor.
  // Since the icon representation is precached, it should be returned right
  // away. Also, we should not receive any notifications.
  gfx::ImageSkiaRep representation =
      image.image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_16));

  EXPECT_EQ(0, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());
}

// Tests behaviour of image created by IconImage after IconImage host goes
// away. The image should still return loaded representations. If requested
// representation was not loaded while IconImage host was around, transparent
// representations should be returned.
TEST_F(ExtensionIconImageTest, IconImageDestruction) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 =
      GetTestBitmap(extension, "16.png", 16, ImageLoadingTracker::DONT_CACHE);
  ASSERT_FALSE(bitmap_16.empty());

  scoped_ptr<IconImage> image(
      new IconImage(extension, extension->icons(), 16, default_icon, this));

  // Load an image representation.
  gfx::ImageSkiaRep representation =
      image->image_skia().GetRepresentation(ui::SCALE_FACTOR_100P);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image->image_skia().image_reps().size());

  // Stash loaded image skia, and destroy |image|.
  gfx::ImageSkia image_skia = image->image_skia();
  image.reset();
  extension = NULL;

  // Image skia should still be able to get previously loaded representation.
  representation = image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);

  EXPECT_EQ(ui::SCALE_FACTOR_100P, representation.scale_factor());
  EXPECT_EQ(16, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_16));

  // When requesting another representation, we should get blank image.
  representation = image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);

  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(16, ui::SCALE_FACTOR_200P)));
}
