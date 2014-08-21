// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_file_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::Manifest;

// Our test class that takes care of managing the necessary threads for loading
// extension icons, and waiting for those loads to happen.
class ExtensionIconManagerTest : public testing::Test {
 public:
  ExtensionIconManagerTest() :
      unwaited_image_loads_(0),
      waiting_(false),
      ui_thread_(BrowserThread::UI, &ui_loop_),
      file_thread_(BrowserThread::FILE),
      io_thread_(BrowserThread::IO) {}

  virtual ~ExtensionIconManagerTest() {}

  void ImageLoadObserved() {
    unwaited_image_loads_++;
    if (waiting_) {
      base::MessageLoop::current()->Quit();
    }
  }

  void WaitForImageLoad() {
    if (unwaited_image_loads_ == 0) {
      waiting_ = true;
      base::MessageLoop::current()->Run();
      waiting_ = false;
    }
    ASSERT_GT(unwaited_image_loads_, 0);
    unwaited_image_loads_--;
  }

 private:
  virtual void SetUp() {
    file_thread_.Start();
    io_thread_.Start();
  }

  // The number of observed image loads that have not been waited for.
  int unwaited_image_loads_;

  // Whether we are currently waiting for an image load.
  bool waiting_;

  base::MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconManagerTest);
};

// This is a specialization of ExtensionIconManager, with a special override to
// call back to the test when an icon has completed loading.
class TestIconManager : public ExtensionIconManager {
 public:
  explicit TestIconManager(ExtensionIconManagerTest* test) : test_(test) {}
  virtual ~TestIconManager() {}

  // Overrides the ImageLoader callback, and calls through to the base class'
  // implementation. Then it lets the test know that an image load was observed.
  virtual void OnImageLoaded(const std::string& extension_id,
                             const gfx::Image& image) OVERRIDE {
    ExtensionIconManager::OnImageLoaded(extension_id, image);
    test_->ImageLoadObserved();
  }

 private:
  ExtensionIconManagerTest* test_;

  DISALLOW_COPY_AND_ASSIGN(TestIconManager);
};

// Returns the default icon that ExtensionIconManager gives when an extension
// doesn't have an icon.
SkBitmap GetDefaultIcon() {
  std::string dummy_id = crx_file::id_util::GenerateId("whatever");
  ExtensionIconManager manager;
  return manager.GetIcon(dummy_id);
}

// Tests loading an icon for an extension, removing it, then re-loading it.
TEST_F(ExtensionIconManagerTest, LoadRemoveLoad) {
  scoped_ptr<Profile> profile(new TestingProfile());
  SkBitmap default_icon = GetDefaultIcon();

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath manifest_path = test_dir.AppendASCII(
      "extensions/image_loading_tracker/app.json");

  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<base::DictionaryValue> manifest(
      static_cast<base::DictionaryValue*>(serializer.Deserialize(NULL, NULL)));
  ASSERT_TRUE(manifest.get() != NULL);

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      manifest_path.DirName(), Manifest::INVALID_LOCATION, *manifest.get(),
      Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get());
  TestIconManager icon_manager(this);

  // Load the icon and grab the bitmap.
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  SkBitmap first_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::BitmapsAreEqual(first_icon, default_icon));

  // Remove the icon from the manager.
  icon_manager.RemoveIcon(extension->id());

  // Now re-load the icon - we should get the same result bitmap (and not the
  // default icon).
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  SkBitmap second_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::BitmapsAreEqual(second_icon, default_icon));

  EXPECT_TRUE(gfx::BitmapsAreEqual(first_icon, second_icon));
}

#if defined(OS_CHROMEOS)
// Tests loading an icon for a component extension.
TEST_F(ExtensionIconManagerTest, LoadComponentExtensionResource) {
  scoped_ptr<Profile> profile(new TestingProfile());
  SkBitmap default_icon = GetDefaultIcon();

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath manifest_path = test_dir.AppendASCII(
      "extensions/file_manager/app.json");

  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<base::DictionaryValue> manifest(
      static_cast<base::DictionaryValue*>(serializer.Deserialize(NULL, NULL)));
  ASSERT_TRUE(manifest.get() != NULL);

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      manifest_path.DirName(), Manifest::COMPONENT, *manifest.get(),
      Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get());

  TestIconManager icon_manager(this);
  // Load the icon and grab the bitmap.
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  SkBitmap first_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::BitmapsAreEqual(first_icon, default_icon));

  // Remove the icon from the manager.
  icon_manager.RemoveIcon(extension->id());

  // Now re-load the icon - we should get the same result bitmap (and not the
  // default icon).
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  SkBitmap second_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::BitmapsAreEqual(second_icon, default_icon));

  EXPECT_TRUE(gfx::BitmapsAreEqual(first_icon, second_icon));
}
#endif
