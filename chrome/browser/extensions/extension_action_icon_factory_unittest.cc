// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_icon_factory.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/extension.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using content::BrowserThread;

namespace extensions {
namespace {

bool ImageRepsAreEqual(const gfx::ImageSkiaRep& image_rep1,
                       const gfx::ImageSkiaRep& image_rep2) {
  return image_rep1.scale() == image_rep2.scale() &&
         gfx::BitmapsAreEqual(image_rep1.sk_bitmap(), image_rep2.sk_bitmap());
}

gfx::Image EnsureImageSize(const gfx::Image& original, int size) {
  const SkBitmap* original_bitmap = original.ToSkBitmap();
  if (original_bitmap->width() == size && original_bitmap->height() == size)
    return original;

  SkBitmap resized = skia::ImageOperations::Resize(
      *original.ToSkBitmap(), skia::ImageOperations::RESIZE_LANCZOS3,
      size, size);
  return gfx::Image::CreateFrom1xBitmap(resized);
}

gfx::ImageSkiaRep CreateBlankRep(int size_dip, float scale) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(static_cast<int>(size_dip * scale),
                        static_cast<int>(size_dip * scale));
  bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));
  return gfx::ImageSkiaRep(bitmap, scale);
}

gfx::Image LoadIcon(const std::string& filename) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions/api_test").AppendASCII(filename);

  std::string file_contents;
  base::ReadFileToString(path, &file_contents);
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(file_contents.data());

  SkBitmap bitmap;
  gfx::PNGCodec::Decode(data, file_contents.length(), &bitmap);

  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

class ExtensionActionIconFactoryTest
    : public testing::Test,
      public ExtensionActionIconFactory::Observer {
 public:
  ExtensionActionIconFactoryTest()
      : quit_in_icon_updated_(false),
        ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {
  }

  virtual ~ExtensionActionIconFactoryTest() {}

  void WaitForIconUpdate() {
    quit_in_icon_updated_ = true;
    base::MessageLoop::current()->Run();
    quit_in_icon_updated_ = false;
  }

  scoped_refptr<Extension> CreateExtension(const char* name,
                                           Manifest::Location location) {
    // Create and load an extension.
    base::FilePath test_file;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_file)) {
      EXPECT_FALSE(true);
      return NULL;
    }
    test_file = test_file.AppendASCII("extensions/api_test").AppendASCII(name);
    int error_code = 0;
    std::string error;
    JSONFileValueSerializer serializer(test_file.AppendASCII("manifest.json"));
    scoped_ptr<base::DictionaryValue> valid_value(
        static_cast<base::DictionaryValue*>(serializer.Deserialize(&error_code,
                                                                   &error)));
    EXPECT_EQ(0, error_code) << error;
    if (error_code != 0)
      return NULL;

    EXPECT_TRUE(valid_value.get());
    if (!valid_value)
      return NULL;

    scoped_refptr<Extension> extension =
        Extension::Create(test_file, location, *valid_value,
                          Extension::NO_FLAGS, &error);
    EXPECT_TRUE(extension.get()) << error;
    if (extension.get())
      extension_service_->AddExtension(extension.get());
    return extension;
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    file_thread_.Start();
    io_thread_.Start();
    profile_.reset(new TestingProfile);
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extension_service_ = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile_.get()))->
        CreateExtensionService(&command_line, base::FilePath(), false);
  }

  virtual void TearDown() OVERRIDE {
    profile_.reset();  // Get all DeleteSoon calls sent to ui_loop_.
    ui_loop_.RunUntilIdle();
  }

  // ExtensionActionIconFactory::Observer overrides:
  virtual void OnIconUpdated() OVERRIDE {
    if (quit_in_icon_updated_)
      base::MessageLoop::current()->Quit();
  }

  gfx::ImageSkia GetFavicon() {
    return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_EXTENSIONS_FAVICON);
  }

  ExtensionAction* GetBrowserAction(const Extension& extension) {
    return ExtensionActionManager::Get(profile())->GetBrowserAction(extension);
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  bool quit_in_icon_updated_;
  base::MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionIconFactoryTest);
};

// If there is no default icon, and the icon has not been set using |SetIcon|,
// the factory should return favicon.
TEST_F(ExtensionActionIconFactoryTest, NoIcons) {
  // Load an extension that has browser action without default icon set in the
  // manifest and does not call |SetIcon| by default.
  scoped_refptr<Extension> extension(CreateExtension(
      "browser_action/no_icon", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);
  ExtensionAction* browser_action = GetBrowserAction(*extension.get());
  ASSERT_TRUE(browser_action);
  ASSERT_FALSE(browser_action->default_icon());
  ASSERT_TRUE(browser_action->GetExplicitlySetIcon(0 /*tab id*/).isNull());

  gfx::ImageSkia favicon = GetFavicon();

  ExtensionActionIconFactory icon_factory(
      profile(), extension.get(), browser_action, this);

  gfx::Image icon = icon_factory.GetIcon(0);

  EXPECT_TRUE(ImageRepsAreEqual(
      favicon.GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));
}

// If the icon has been set using |SetIcon|, the factory should return that
// icon.
TEST_F(ExtensionActionIconFactoryTest, AfterSetIcon) {
  // Load an extension that has browser action without default icon set in the
  // manifest and does not call |SetIcon| by default (but has an browser action
  // icon resource).
  scoped_refptr<Extension> extension(CreateExtension(
      "browser_action/no_icon", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);
  ExtensionAction* browser_action = GetBrowserAction(*extension.get());
  ASSERT_TRUE(browser_action);
  ASSERT_FALSE(browser_action->default_icon());
  ASSERT_TRUE(browser_action->GetExplicitlySetIcon(0 /*tab id*/).isNull());

  gfx::Image set_icon = LoadIcon("browser_action/no_icon/icon.png");
  ASSERT_FALSE(set_icon.IsEmpty());

  browser_action->SetIcon(0, set_icon);

  ASSERT_FALSE(browser_action->GetExplicitlySetIcon(0 /*tab id*/).isNull());

  ExtensionActionIconFactory icon_factory(
      profile(), extension.get(), browser_action, this);

  gfx::Image icon = icon_factory.GetIcon(0);

  EXPECT_TRUE(ImageRepsAreEqual(
      set_icon.ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  // It should still return favicon for another tabs.
  icon = icon_factory.GetIcon(1);

  EXPECT_TRUE(ImageRepsAreEqual(
      GetFavicon().GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));
}

// If there is a default icon, and the icon has not been set using |SetIcon|,
// the factory should return the default icon.
TEST_F(ExtensionActionIconFactoryTest, DefaultIcon) {
  // Load an extension that has browser action without default icon set in the
  // manifest and does not call |SetIcon| by default (but has an browser action
  // icon resource).
  scoped_refptr<Extension> extension(CreateExtension(
      "browser_action/no_icon", Manifest::INVALID_LOCATION));
  ASSERT_TRUE(extension.get() != NULL);
  ExtensionAction* browser_action = GetBrowserAction(*extension.get());
  ASSERT_TRUE(browser_action);
  ASSERT_FALSE(browser_action->default_icon());
  ASSERT_TRUE(browser_action->GetExplicitlySetIcon(0 /*tab id*/).isNull());

  gfx::Image default_icon =
      EnsureImageSize(LoadIcon("browser_action/no_icon/icon.png"), 19);
  ASSERT_FALSE(default_icon.IsEmpty());

  scoped_ptr<ExtensionIconSet> default_icon_set(new ExtensionIconSet());
  default_icon_set->Add(19, "icon.png");

  browser_action->set_default_icon(default_icon_set.Pass());
  ASSERT_TRUE(browser_action->default_icon());

  ExtensionActionIconFactory icon_factory(
      profile(), extension.get(), browser_action, this);

  gfx::Image icon = icon_factory.GetIcon(0);

  // The icon should be loaded asynchronously. Initially a transparent icon
  // should be returned.
  EXPECT_TRUE(ImageRepsAreEqual(
      CreateBlankRep(19, 1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  WaitForIconUpdate();

  icon = icon_factory.GetIcon(0);

  // The default icon representation should be loaded at this point.
  EXPECT_TRUE(ImageRepsAreEqual(
      default_icon.ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

  // The same icon should be returned for the other tabs.
  icon = icon_factory.GetIcon(1);

  EXPECT_TRUE(ImageRepsAreEqual(
      default_icon.ToImageSkia()->GetRepresentation(1.0f),
      icon.ToImageSkia()->GetRepresentation(1.0f)));

}

}  // namespace
}  // namespace extensions
