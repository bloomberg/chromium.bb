// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_icon_image.h"

#include "base/json/json_file_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::IconImage;
using extensions::Manifest;

namespace {

SkBitmap CreateBlankBitmapForScale(int size_dip, ui::ScaleFactor scale_factor) {
  SkBitmap bitmap;
  const float scale = ui::GetScaleForScaleFactor(scale_factor);
  bitmap.allocN32Pixels(static_cast<int>(size_dip * scale),
                        static_cast<int>(size_dip * scale));
  bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));
  return bitmap;
}

SkBitmap EnsureBitmapSize(const SkBitmap& original, int size) {
  if (original.width() == size && original.height() == size)
    return original;

  SkBitmap resized = skia::ImageOperations::Resize(
      original, skia::ImageOperations::RESIZE_LANCZOS3, size, size);
  return resized;
}

// Used to test behavior including images defined by an image skia source.
// |GetImageForScale| simply returns image representation from the image given
// in the ctor.
class MockImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  explicit MockImageSkiaSource(const gfx::ImageSkia& image)
      : image_(image) {
  }
  virtual ~MockImageSkiaSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(float scale) OVERRIDE {
    return image_.GetRepresentation(scale);
  }

 private:
  gfx::ImageSkia image_;
};

// Helper class for synchronously loading extension image resource.
class TestImageLoader {
 public:
  explicit TestImageLoader(const Extension* extension)
      : extension_(extension),
        waiting_(false),
        image_loaded_(false) {
  }
  virtual ~TestImageLoader() {}

  void OnImageLoaded(const gfx::Image& image) {
    image_ = image;
    image_loaded_ = true;
    if (waiting_)
      base::MessageLoop::current()->Quit();
  }

  SkBitmap LoadBitmap(const std::string& path,
                      int size) {
    image_loaded_ = false;

    image_loader_.LoadImageAsync(
        extension_, extension_->GetResource(path), gfx::Size(size, size),
        base::Bind(&TestImageLoader::OnImageLoaded,
                   base::Unretained(this)));

    // If |image_| still hasn't been loaded (i.e. it is being loaded
    // asynchronously), wait for it.
    if (!image_loaded_) {
      waiting_ = true;
      base::MessageLoop::current()->Run();
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
  extensions::ImageLoader image_loader_;

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
    base::MessageLoop::current()->Run();
    quit_in_image_loaded_ = false;
  }

  int ImageLoadedCount() {
    int result = image_loaded_count_;
    image_loaded_count_ = 0;
    return result;
  }

  scoped_refptr<Extension> CreateExtension(const char* name,
                                           Manifest::Location location) {
    // Create and load an extension.
    base::FilePath test_file;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_file)) {
      EXPECT_FALSE(true);
      return NULL;
    }
    test_file = test_file.AppendASCII("extensions").AppendASCII(name);
    int error_code = 0;
    std::string error;
    JSONFileValueSerializer serializer(test_file.AppendASCII("app.json"));
    scoped_ptr<base::DictionaryValue> valid_value(
        static_cast<base::DictionaryValue*>(serializer.Deserialize(&error_code,
                                                                   &error)));
    EXPECT_EQ(0, error_code) << error;
    if (error_code != 0)
      return NULL;

    EXPECT_TRUE(valid_value.get());
    if (!valid_value)
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
      base::MessageLoop::current()->Quit();
  }

  gfx::ImageSkia GetDefaultIcon() {
    return gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(16, 16), 1.0f));
  }

  // Loads an image to be used in test from the extension.
  // The image will be loaded from the relative path |path|.
  SkBitmap GetTestBitmap(const Extension* extension,
                         const std::string& path,
                         int size) {
    TestImageLoader image_loader(extension);
    return image_loader.LoadBitmap(path, size);
  }

 private:
  int image_loaded_count_;
  bool quit_in_image_loaded_;
  base::MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconImageTest);
};

}  // namespace

TEST_F(ExtensionIconImageTest, Basic) {
  std::vector<ui::ScaleFactor> supported_factors;
  supported_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 = GetTestBitmap(extension.get(), "16.png", 16);
  ASSERT_FALSE(bitmap_16.empty());

  // There is no image of size 32 defined in the extension manifest, so we
  // should expect manifest image of size 48 resized to size 32.
  SkBitmap bitmap_48_resized_to_32 =
      GetTestBitmap(extension.get(), "48.png", 32);
  ASSERT_FALSE(bitmap_48_resized_to_32.empty());

  IconImage image(profile.get(),
                  extension.get(),
                  extensions::IconsInfo::GetIcons(extension.get()),
                  16,
                  default_icon,
                  this);

  // No representations in |image_| yet.
  gfx::ImageSkia::ImageSkiaReps image_reps = image.image_skia().image_reps();
  ASSERT_EQ(0u, image_reps.size());

  // Gets representation for a scale factor.
  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  // Before the image representation is loaded, image should contain blank
  // image representation.
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(16, ui::SCALE_FACTOR_100P)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);

  // We should get the right representation now.
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_16));
  EXPECT_EQ(16, representation.pixel_width());

  // Gets representation for an additional scale factor.
  representation = image.image_skia().GetRepresentation(2.0f);

  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(16, ui::SCALE_FACTOR_200P)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(2u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(2.0f);

  // Image should have been resized.
  EXPECT_EQ(32, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(),
                                   bitmap_48_resized_to_32));
}

// There is no resource with either exact or bigger size, but there is a smaller
// resource.
TEST_F(ExtensionIconImageTest, FallbackToSmallerWhenNoBigger) {
  std::vector<ui::ScaleFactor> supported_factors;
  supported_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_48 = GetTestBitmap(extension.get(), "48.png", 48);
  ASSERT_FALSE(bitmap_48.empty());

  IconImage image(profile.get(),
                  extension.get(),
                  extensions::IconsInfo::GetIcons(extension.get()),
                  32,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(2.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(2.0f);

  // We should have loaded the biggest smaller resource resized to the actual
  // size.
  EXPECT_EQ(2.0f, representation.scale());
  EXPECT_EQ(64, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(),
                                   EnsureBitmapSize(bitmap_48, 64)));
}

// There is no resource with exact size, but there is a smaller and a bigger
// one. Requested size is smaller than 32 though, so the smaller resource should
// be loaded.
TEST_F(ExtensionIconImageTest, FallbackToSmaller) {
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 = GetTestBitmap(extension.get(), "16.png", 16);
  ASSERT_FALSE(bitmap_16.empty());

  IconImage image(profile.get(),
                  extension.get(),
                  extensions::IconsInfo::GetIcons(extension.get()),
                  17,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);

  // We should have loaded smaller (resized) resource.
  EXPECT_EQ(1.0f, representation.scale());
  EXPECT_EQ(17, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(),
                                   EnsureBitmapSize(bitmap_16, 17)));
}

// If resource set is empty, |GetRepresentation| should synchronously return
// default icon, without notifying observer of image change.
TEST_F(ExtensionIconImageTest, NoResources) {
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionIconSet empty_icon_set;
  gfx::ImageSkia default_icon = GetDefaultIcon();

  const int kRequestedSize = 24;
  IconImage image(profile.get(),
                  extension.get(),
                  empty_icon_set,
                  kRequestedSize,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      EnsureBitmapSize(
          default_icon.GetRepresentation(1.0f).sk_bitmap(),
          kRequestedSize)));

  EXPECT_EQ(0, ImageLoadedCount());
  // We should have a default icon representation.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      EnsureBitmapSize(
          default_icon.GetRepresentation(1.0f).sk_bitmap(),
          kRequestedSize)));
}

// If resource set is invalid, image load should be done asynchronously and
// the observer should be notified when it's done. |GetRepresentation| should
// return the default icon representation once image load is done.
TEST_F(ExtensionIconImageTest, InvalidResource) {
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  const int kInvalidIconSize = 24;
  ExtensionIconSet invalid_icon_set;
  invalid_icon_set.Add(kInvalidIconSize, "invalid.png");

  gfx::ImageSkia default_icon = GetDefaultIcon();

  IconImage image(profile.get(),
                  extension.get(),
                  invalid_icon_set,
                  kInvalidIconSize,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      CreateBlankBitmapForScale(kInvalidIconSize, ui::SCALE_FACTOR_100P)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  // We should have default icon representation now.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      EnsureBitmapSize(
          default_icon.GetRepresentation(1.0f).sk_bitmap(),
          kInvalidIconSize)));
}

// Test that IconImage works with lazily (but synchronously) created default
// icon when IconImage returns synchronously.
TEST_F(ExtensionIconImageTest, LazyDefaultIcon) {
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();
  gfx::ImageSkia lazy_default_icon(new MockImageSkiaSource(default_icon),
                                    default_icon.size());

  ExtensionIconSet empty_icon_set;

  const int kRequestedSize = 128;
  IconImage image(profile.get(),
                  extension.get(),
                  empty_icon_set,
                  kRequestedSize,
                  lazy_default_icon,
                  this);

  ASSERT_FALSE(lazy_default_icon.HasRepresentation(1.0f));

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  // The resouce set is empty, so we should get the result right away.
  EXPECT_TRUE(lazy_default_icon.HasRepresentation(1.0f));
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      EnsureBitmapSize(
          default_icon.GetRepresentation(1.0f).sk_bitmap(),
          kRequestedSize)));

  // We should have a default icon representation.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());
}

// Test that IconImage works with lazily (but synchronously) created default
// icon when IconImage returns asynchronously.
TEST_F(ExtensionIconImageTest, LazyDefaultIcon_AsyncIconImage) {
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();
  gfx::ImageSkia lazy_default_icon(new MockImageSkiaSource(default_icon),
                                    default_icon.size());

  const int kInvalidIconSize = 24;
  ExtensionIconSet invalid_icon_set;
  invalid_icon_set.Add(kInvalidIconSize, "invalid.png");

  IconImage image(profile.get(),
                  extension.get(),
                  invalid_icon_set,
                  kInvalidIconSize,
                  lazy_default_icon,
                  this);

  ASSERT_FALSE(lazy_default_icon.HasRepresentation(1.0f));

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  // We should have default icon representation now.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  EXPECT_TRUE(lazy_default_icon.HasRepresentation(1.0f));

  representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.sk_bitmap(),
      EnsureBitmapSize(
          default_icon.GetRepresentation(1.0f).sk_bitmap(),
          kInvalidIconSize)));
}

// Tests behavior of image created by IconImage after IconImage host goes
// away. The image should still return loaded representations. If requested
// representation was not loaded while IconImage host was around, transparent
// representations should be returned.
TEST_F(ExtensionIconImageTest, IconImageDestruction) {
  scoped_ptr<content::BrowserContext> profile(new TestingProfile());
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 = GetTestBitmap(extension.get(), "16.png", 16);
  ASSERT_FALSE(bitmap_16.empty());

  scoped_ptr<IconImage> image(
      new IconImage(profile.get(),
                    extension.get(),
                    extensions::IconsInfo::GetIcons(extension.get()),
                    16,
                    default_icon,
                    this));

  // Load an image representation.
  gfx::ImageSkiaRep representation =
      image->image_skia().GetRepresentation(1.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image->image_skia().image_reps().size());

  // Stash loaded image skia, and destroy |image|.
  gfx::ImageSkia image_skia = image->image_skia();
  image.reset();
  extension = NULL;

  // Image skia should still be able to get previously loaded representation.
  representation = image_skia.GetRepresentation(1.0f);

  EXPECT_EQ(1.0f, representation.scale());
  EXPECT_EQ(16, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.sk_bitmap(), bitmap_16));

  // When requesting another representation, we should not crash and return some
  // image of the size. It could be blank or a rescale from the existing 1.0f
  // icon.
  representation = image_skia.GetRepresentation(2.0f);

  EXPECT_EQ(16, representation.GetWidth());
  EXPECT_EQ(16, representation.GetHeight());
  EXPECT_EQ(2.0f, representation.scale());
}
