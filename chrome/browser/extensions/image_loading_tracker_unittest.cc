// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "grit/component_extension_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"

using content::BrowserThread;

class ImageLoadingTrackerTest : public testing::Test,
                                public ImageLoadingTracker::Observer {
 public:
  ImageLoadingTrackerTest()
      : image_loaded_count_(0),
        quit_in_image_loaded_(false),
        ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {
  }

  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE {
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
                                           Extension::Location location) {
    // Create and load an extension.
    FilePath test_file;
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

    return Extension::Create(test_file, location, *valid_value,
        Extension::STRICT_ERROR_CHECKS, &error);
  }

  gfx::Image image_;

 private:
  virtual void SetUp() {
    file_thread_.Start();
    io_thread_.Start();
  }

  int image_loaded_count_;
  bool quit_in_image_loaded_;
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
};

// Tests asking ImageLoadingTracker to cache pushes the result to the Extension.
TEST_F(ImageLoadingTrackerTest, Cache) {
  scoped_refptr<Extension> extension(CreateExtension(
      "image_loading_tracker", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionResource image_resource =
      extension->GetIconResource(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                                 ExtensionIconSet::MATCH_EXACTLY);
  gfx::Size max_size(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                     ExtensionIconSet::EXTENSION_ICON_SMALLISH);
  ImageLoadingTracker loader(this);
  loader.LoadImage(extension.get(),
                   image_resource,
                   max_size,
                   ImageLoadingTracker::CACHE);

  // The image isn't cached, so we should not have received notification.
  EXPECT_EQ(0, image_loaded_count());

  WaitForImageLoad();

  // We should have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that the image was loaded.
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
            image_.ToSkBitmap()->width());

  // The image should be cached in the Extension.
  EXPECT_TRUE(extension->HasCachedImage(image_resource, max_size));

  // Make sure the image is in the extension.
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
            extension->GetCachedImage(image_resource, max_size).width());

  // Ask the tracker for the image again, this should call us back immediately.
  loader.LoadImage(extension.get(),
                   image_resource,
                   max_size,
                   ImageLoadingTracker::CACHE);
  // We should have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that the image was loaded.
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
            image_.ToSkBitmap()->width());
}

// Tests deleting an extension while waiting for the image to load doesn't cause
// problems.
TEST_F(ImageLoadingTrackerTest, DeleteExtensionWhileWaitingForCache) {
  scoped_refptr<Extension> extension(CreateExtension(
      "image_loading_tracker", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionResource image_resource =
      extension->GetIconResource(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                                 ExtensionIconSet::MATCH_EXACTLY);
  ImageLoadingTracker loader(this);
  loader.LoadImage(extension.get(),
                   image_resource,
                   gfx::Size(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                             ExtensionIconSet::EXTENSION_ICON_SMALLISH),
                   ImageLoadingTracker::CACHE);

  // The image isn't cached, so we should not have received notification.
  EXPECT_EQ(0, image_loaded_count());

  // Send out notification the extension was uninstalled.
  UnloadedExtensionInfo details(extension.get(),
                                extension_misc::UNLOAD_REASON_UNINSTALL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::NotificationService::AllSources(),
      content::Details<UnloadedExtensionInfo>(&details));

  // Chuck the extension, that way if anyone tries to access it we should crash
  // or get valgrind errors.
  extension = NULL;

  WaitForImageLoad();

  // Even though we deleted the extension, we should still get the image.
  // We should still have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that the image was loaded.
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
            image_.ToSkBitmap()->width());
}

// Tests loading multiple dimensions of the same image.
TEST_F(ImageLoadingTrackerTest, MultipleImages) {
  scoped_refptr<Extension> extension(CreateExtension(
      "image_loading_tracker", Extension::INVALID));
  ASSERT_TRUE(extension.get() != NULL);

  std::vector<ImageLoadingTracker::ImageInfo> info_list;
  int sizes[] = {ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                 ExtensionIconSet::EXTENSION_ICON_BITTY};
  for (size_t i = 0; i < arraysize(sizes); ++i) {
    ExtensionResource resource =
        extension->GetIconResource(sizes[i], ExtensionIconSet::MATCH_EXACTLY);
    info_list.push_back(ImageLoadingTracker::ImageInfo(
        resource, gfx::Size(sizes[i], sizes[i])));
  }

  ImageLoadingTracker loader(this);
  loader.LoadImages(extension.get(), info_list, ImageLoadingTracker::CACHE);

  // The image isn't cached, so we should not have received notification.
  EXPECT_EQ(0, image_loaded_count());

  WaitForImageLoad();

  // We should have gotten the image.
  EXPECT_EQ(1, image_loaded_count());

  // Check that all images were loaded.
  const std::vector<SkBitmap> bitmaps = image_.ToImageSkia()->bitmaps();
  ASSERT_EQ(2u, bitmaps.size());
  const SkBitmap* bmp1 = &bitmaps[0];
  const SkBitmap* bmp2 = &bitmaps[1];
  if (bmp1->width() > bmp2->width()) {
    std::swap(bmp1, bmp2);
  }
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_BITTY, bmp1->width());
  EXPECT_EQ(ExtensionIconSet::EXTENSION_ICON_SMALLISH, bmp2->width());
}

// Tests IsComponentExtensionResource function.
TEST_F(ImageLoadingTrackerTest, IsComponentExtensionResource) {
  scoped_refptr<Extension> extension(CreateExtension(
      "file_manager", Extension::COMPONENT));
  ASSERT_TRUE(extension.get() != NULL);

  ExtensionResource resource =
      extension->GetIconResource(ExtensionIconSet::EXTENSION_ICON_BITTY,
                                 ExtensionIconSet::MATCH_EXACTLY);

#if defined(FILE_MANAGER_EXTENSION)
  ImageLoadingTracker loader(this);
  int resource_id;
  ASSERT_EQ(true,
            loader.IsComponentExtensionResource(extension.get(),
                                                resource,
                                                resource_id));
  ASSERT_EQ(IDR_FILE_MANAGER_ICON_16, resource_id);
#endif
}
