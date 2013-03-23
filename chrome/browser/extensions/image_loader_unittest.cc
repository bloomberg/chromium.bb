// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loader.h"

#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/extension_resource.h"
#include "grit/component_extension_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionResource;
using extensions::ImageLoader;
using extensions::Manifest;

class ImageLoaderTest : public testing::Test {
 public:
  ImageLoaderTest()
      : image_loaded_count_(0),
        quit_in_image_loaded_(false),
        ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {
  }

  void OnImageLoaded(const gfx::Image& image) {
    image_loaded_count_++;
    if (quit_in_image_loaded_)
      MessageLoop::current()->Quit();
    image_ = image;
  }

  void WaitForImageLoad() {
    quit_in_image_loaded_ = true;
    MessageLoop::current()->Run();
    quit_in_image_loaded_ = false;
  }

  int image_loaded_count() {
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
    test_file = test_file.AppendASCII("extensions")
                         .AppendASCII(name);
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

    if (location == Manifest::COMPONENT) {
      if (!PathService::Get(chrome::DIR_RESOURCES, &test_file)) {
        EXPECT_FALSE(true);
        return NULL;
      }
      test_file = test_file.AppendASCII(name);
    }
    return Extension::Create(test_file, location, *valid_value,
                             Extension::NO_FLAGS, &error);
  }

  gfx::Image image_;

 private:
  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    (new extensions::IconsHandler)->Register();

    file_thread_.Start();
    io_thread_.Start();
  }

  virtual void TearDown() OVERRIDE {
    extensions::ManifestHandler::ClearRegistryForTesting();
  }

  int image_loaded_count_;
  bool quit_in_image_loaded_;
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
};

// Tests loading an image works correctly.
TEST_F(ImageLoaderTest, LoadImage) {
  scoped_refptr<Extension> extension(CreateExtension(
      "image_loading_tracker", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionResource image_resource = extensions::IconsInfo::GetIconResource(
      extension,
      extension_misc::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_EXACTLY);
  gfx::Size max_size(extension_misc::EXTENSION_ICON_SMALLISH,
                     extension_misc::EXTENSION_ICON_SMALLISH);
  ImageLoader loader;
  loader.LoadImageAsync(extension.get(),
                        image_resource,
                        max_size,
                        base::Bind(&ImageLoaderTest::OnImageLoaded,
                                   base::Unretained(this)));

  // The image isn't cached, so we should not have received notification.
  EXPECT_EQ(0, image_loaded_count());

  WaitForImageLoad();

  // We should have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that the image was loaded.
  EXPECT_EQ(extension_misc::EXTENSION_ICON_SMALLISH,
            image_.ToSkBitmap()->width());
}

// Tests deleting an extension while waiting for the image to load doesn't cause
// problems.
TEST_F(ImageLoaderTest, DeleteExtensionWhileWaitingForCache) {
  scoped_refptr<Extension> extension(CreateExtension(
      "image_loading_tracker", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionResource image_resource = extensions::IconsInfo::GetIconResource(
      extension,
      extension_misc::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_EXACTLY);
  gfx::Size max_size(extension_misc::EXTENSION_ICON_SMALLISH,
                     extension_misc::EXTENSION_ICON_SMALLISH);
  ImageLoader loader;
  std::set<int> sizes;
  sizes.insert(extension_misc::EXTENSION_ICON_SMALLISH);
  loader.LoadImageAsync(extension.get(),
                        image_resource,
                        max_size,
                        base::Bind(&ImageLoaderTest::OnImageLoaded,
                                   base::Unretained(this)));

  // The image isn't cached, so we should not have received notification.
  EXPECT_EQ(0, image_loaded_count());

  // Send out notification the extension was uninstalled.
  extensions::UnloadedExtensionInfo details(extension.get(),
      extension_misc::UNLOAD_REASON_UNINSTALL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::NotificationService::AllSources(),
      content::Details<extensions::UnloadedExtensionInfo>(&details));

  // Chuck the extension, that way if anyone tries to access it we should crash
  // or get valgrind errors.
  extension = NULL;

  WaitForImageLoad();

  // Even though we deleted the extension, we should still get the image.
  // We should still have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that the image was loaded.
  EXPECT_EQ(extension_misc::EXTENSION_ICON_SMALLISH,
            image_.ToSkBitmap()->width());
}

// Tests loading multiple dimensions of the same image.
TEST_F(ImageLoaderTest, MultipleImages) {
  scoped_refptr<Extension> extension(CreateExtension(
      "image_loading_tracker", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);

  std::vector<ImageLoader::ImageRepresentation> info_list;
  int sizes[] = {extension_misc::EXTENSION_ICON_SMALLISH,
                 extension_misc::EXTENSION_ICON_BITTY};
  for (size_t i = 0; i < arraysize(sizes); ++i) {
    ExtensionResource resource = extensions::IconsInfo::GetIconResource(
        extension, sizes[i], ExtensionIconSet::MATCH_EXACTLY);
    info_list.push_back(ImageLoader::ImageRepresentation(
        resource,
        ImageLoader::ImageRepresentation::RESIZE_WHEN_LARGER,
        gfx::Size(sizes[i], sizes[i]),
        ui::SCALE_FACTOR_NONE));
  }

  ImageLoader loader;
  loader.LoadImagesAsync(extension.get(), info_list,
                         base::Bind(&ImageLoaderTest::OnImageLoaded,
                                    base::Unretained(this)));

  // The image isn't cached, so we should not have received notification.
  EXPECT_EQ(0, image_loaded_count());

  WaitForImageLoad();

  // We should have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that all images were loaded.
  std::vector<gfx::ImageSkiaRep> image_reps =
      image_.ToImageSkia()->image_reps();
  ASSERT_EQ(2u, image_reps.size());
  const gfx::ImageSkiaRep* img_rep1 = &image_reps[0];
  const gfx::ImageSkiaRep* img_rep2 = &image_reps[1];
  if (img_rep1->pixel_width() > img_rep2->pixel_width()) {
    std::swap(img_rep1, img_rep2);
  }
  EXPECT_EQ(extension_misc::EXTENSION_ICON_BITTY,
            img_rep1->pixel_width());
  EXPECT_EQ(extension_misc::EXTENSION_ICON_SMALLISH,
            img_rep2->pixel_width());
}

// Tests IsComponentExtensionResource function.
TEST_F(ImageLoaderTest, IsComponentExtensionResource) {
  scoped_refptr<Extension> extension(CreateExtension(
      "file_manager", Manifest::COMPONENT));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionResource resource = extensions::IconsInfo::GetIconResource(
      extension,
      extension_misc::EXTENSION_ICON_BITTY,
      ExtensionIconSet::MATCH_EXACTLY);

#if defined(FILE_MANAGER_EXTENSION)
  int resource_id;
  ASSERT_EQ(true,
            ImageLoader::IsComponentExtensionResource(extension->path(),
                                                      resource.relative_path(),
                                                      &resource_id));
  ASSERT_EQ(IDR_FILE_MANAGER_ICON_16, resource_id);
#endif
}
