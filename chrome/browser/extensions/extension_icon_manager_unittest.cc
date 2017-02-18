// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_icon_manager.h"

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/display/display_list.h"
#include "ui/display/display_switches.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace extensions {
namespace {

class ScopedSetDeviceScaleFactor {
 public:
  explicit ScopedSetDeviceScaleFactor(float scale) {
    display::Display::ResetForceDeviceScaleFactorForTesting();
    // It should be enough just to call Display::SetScaleAndBounds, but on Mac
    // that rounds the scale unless there's a forced device scale factor.
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, base::StringPrintf("%3.2f", scale));
    // This has to be inited after fiddling with the command line.
    test_screen_ = base::MakeUnique<display::test::TestScreen>();
    display::Screen::SetScreenInstance(test_screen_.get());
  }

  ~ScopedSetDeviceScaleFactor() {
    display::Display::ResetForceDeviceScaleFactorForTesting();
    display::Screen::SetScreenInstance(nullptr);
  }

 private:
  std::unique_ptr<display::test::TestScreen> test_screen_;
  base::test::ScopedCommandLine command_line_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetDeviceScaleFactor);
};

// Our test class that takes care of managing the necessary threads for loading
// extension icons, and waiting for those loads to happen.
class ExtensionIconManagerTest : public testing::Test {
 public:
  ExtensionIconManagerTest() : unwaited_image_loads_(0), waiting_(false) {}

  ~ExtensionIconManagerTest() override {}

  void ImageLoadObserved() {
    unwaited_image_loads_++;
    if (waiting_) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  void WaitForImageLoad() {
    if (unwaited_image_loads_ == 0) {
      waiting_ = true;
      base::RunLoop().Run();
      waiting_ = false;
    }
    ASSERT_GT(unwaited_image_loads_, 0);
    unwaited_image_loads_--;
  }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  // The number of observed image loads that have not been waited for.
  int unwaited_image_loads_;

  // Whether we are currently waiting for an image load.
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconManagerTest);
};

// This is a specialization of ExtensionIconManager, with a special override to
// call back to the test when an icon has completed loading.
class TestIconManager : public ExtensionIconManager {
 public:
  explicit TestIconManager(ExtensionIconManagerTest* test) : test_(test) {}
  ~TestIconManager() override {}

  // Overrides the ImageLoader callback, and calls through to the base class'
  // implementation. Then it lets the test know that an image load was observed.
  void OnImageLoaded(const std::string& extension_id,
                     const gfx::Image& image) override {
    ExtensionIconManager::OnImageLoaded(extension_id, image);
    test_->ImageLoadObserved();
  }

 private:
  ExtensionIconManagerTest* test_;

  DISALLOW_COPY_AND_ASSIGN(TestIconManager);
};

// Returns the default icon that ExtensionIconManager gives when an extension
// doesn't have an icon.
gfx::Image GetDefaultIcon() {
  std::string dummy_id = crx_file::id_util::GenerateId("whatever");
  ExtensionIconManager manager;
  return manager.GetIcon(dummy_id);
}

// Tests loading an icon for an extension, removing it, then re-loading it.
TEST_F(ExtensionIconManagerTest, LoadRemoveLoad) {
  std::unique_ptr<Profile> profile(new TestingProfile());
  gfx::Image default_icon = GetDefaultIcon();

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath manifest_path = test_dir.AppendASCII(
      "extensions/image_loading_tracker/app.json");

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::DictionaryValue> manifest =
      base::DictionaryValue::From(deserializer.Deserialize(NULL, NULL));
  ASSERT_TRUE(manifest.get() != NULL);

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(manifest_path.DirName(), Manifest::INVALID_LOCATION,
                        *manifest, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get());
  TestIconManager icon_manager(this);

  // Load the icon.
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  gfx::Image first_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::test::AreImagesEqual(first_icon, default_icon));

  // Remove the icon from the manager.
  icon_manager.RemoveIcon(extension->id());

  // Now re-load the icon - we should get the same result bitmap (and not the
  // default icon).
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  gfx::Image second_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::test::AreImagesEqual(second_icon, default_icon));

  EXPECT_TRUE(gfx::test::AreImagesEqual(first_icon, second_icon));
}

#if defined(OS_CHROMEOS)
// Tests loading an icon for a component extension.
TEST_F(ExtensionIconManagerTest, LoadComponentExtensionResource) {
  std::unique_ptr<Profile> profile(new TestingProfile());
  gfx::Image default_icon = GetDefaultIcon();

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath manifest_path = test_dir.AppendASCII(
      "extensions/file_manager/app.json");

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::DictionaryValue> manifest =
      base::DictionaryValue::From(deserializer.Deserialize(NULL, NULL));
  ASSERT_TRUE(manifest.get() != NULL);

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      manifest_path.DirName(), Manifest::COMPONENT, *manifest.get(),
      Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get());

  TestIconManager icon_manager(this);
  // Load the icon.
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  gfx::Image first_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::test::AreImagesEqual(first_icon, default_icon));

  // Remove the icon from the manager.
  icon_manager.RemoveIcon(extension->id());

  // Now re-load the icon - we should get the same result bitmap (and not the
  // default icon).
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  gfx::Image second_icon = icon_manager.GetIcon(extension->id());
  EXPECT_FALSE(gfx::test::AreImagesEqual(second_icon, default_icon));

  EXPECT_TRUE(gfx::test::AreImagesEqual(first_icon, second_icon));
}
#endif

// Test what bitmaps are loaded when various combinations of scale factors are
// supported.
TEST_F(ExtensionIconManagerTest, ScaleFactors) {
  auto profile = base::MakeUnique<TestingProfile>();
  const gfx::Image default_icon = GetDefaultIcon();

  base::FilePath test_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath manifest_path =
      test_dir.AppendASCII("extensions/context_menus/icons/manifest.json");

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::DictionaryValue> manifest =
      base::DictionaryValue::From(deserializer.Deserialize(nullptr, nullptr));
  ASSERT_TRUE(manifest);

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(manifest_path.DirName(), Manifest::INVALID_LOCATION,
                        *manifest, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension);

  constexpr int kMaxIconSizeInManifest = 32;
  std::vector<std::vector<ui::ScaleFactor>> supported_scales = {
      // Base case.
      {ui::SCALE_FACTOR_100P},
      // Two scale factors.
      {ui::SCALE_FACTOR_100P, ui::SCALE_FACTOR_200P},
      // A scale factor that is in between two of the provided icon sizes
      // (should use the larger one and scale down).
      {ui::SCALE_FACTOR_125P},
      // One scale factor for which we have an icon, one scale factor for which
      // we don't.
      {ui::SCALE_FACTOR_100P, ui::SCALE_FACTOR_300P},
      // Just a scale factor where we don't have any icon. This falls back to
      // the default icon.
      {ui::SCALE_FACTOR_300P}};

  for (size_t i = 0; i < supported_scales.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "Test case: " << i);
    // Since active Displays' scale factors are also taken into account, to make
    // the logic in this test work, we need to set the scale factor to one of
    // the "supported" scales.
    ScopedSetDeviceScaleFactor scoped_dsf(
        ui::GetScaleForScaleFactor(supported_scales[i][0]));
    ui::test::ScopedSetSupportedScaleFactors scoped(supported_scales[i]);
    TestIconManager icon_manager(this);

    icon_manager.LoadIcon(profile.get(), extension.get());
    WaitForImageLoad();
    gfx::Image icon = icon_manager.GetIcon(extension->id());
    // Determine if the default icon fallback will be used. We'll use the
    // default when none of the supported scale factors can find an appropriate
    // icon.
    bool should_fall_back_to_default = true;
    for (auto supported_scale : supported_scales[i]) {
      if (gfx::kFaviconSize * ui::GetScaleForScaleFactor(supported_scale) <=
          kMaxIconSizeInManifest) {
        should_fall_back_to_default = false;
        break;
      }
    }
    if (should_fall_back_to_default) {
      EXPECT_TRUE(gfx::test::AreImagesEqual(icon, default_icon));
      continue;
    }

    gfx::ImageSkia image_skia = icon.AsImageSkia();

    for (int scale_factor_iter = ui::SCALE_FACTOR_NONE + 1;
         scale_factor_iter < ui::NUM_SCALE_FACTORS; ++scale_factor_iter) {
      auto scale_factor = static_cast<ui::ScaleFactor>(scale_factor_iter);
      float scale = ui::GetScaleForScaleFactor(scale_factor);
      SCOPED_TRACE(testing::Message() << "Scale: " << scale);

      const bool has_representation = image_skia.HasRepresentation(scale);
      // We shouldn't have a representation if the extension didn't provide a
      // big enough icon.
      if (gfx::kFaviconSize * scale > kMaxIconSizeInManifest)
        EXPECT_FALSE(has_representation);
      else
        EXPECT_EQ(ui::IsSupportedScale(scale), has_representation);
    }
  }

  // Now check that the scale factors for active displays are respected, even
  // when it's not a supported scale.
  EXPECT_FALSE(ui::IsSupportedScale(ui::SCALE_FACTOR_150P));
  ScopedSetDeviceScaleFactor scoped_dsf(1.5f);
  TestIconManager icon_manager(this);
  icon_manager.LoadIcon(profile.get(), extension.get());
  WaitForImageLoad();
  gfx::ImageSkia icon = icon_manager.GetIcon(extension->id()).AsImageSkia();
  EXPECT_TRUE(icon.HasRepresentation(1.5f));
}

}  // namespace
}  // namespace extensions
