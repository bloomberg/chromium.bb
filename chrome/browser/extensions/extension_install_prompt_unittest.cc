// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/skia_util.h"

namespace extensions {

namespace {

void VerifyPromptIconCallback(
    const base::Closure& quit_closure,
    const SkBitmap& expected_bitmap,
    ExtensionInstallPromptShowParams* params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  EXPECT_TRUE(gfx::BitmapsAreEqual(prompt->icon().AsBitmap(), expected_bitmap));
  quit_closure.Run();
}

void VerifyPromptPermissionsCallback(
    const base::Closure& quit_closure,
    size_t regular_permissions_count,
    size_t withheld_permissions_count,
    ExtensionInstallPromptShowParams* params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_ptr<ExtensionInstallPrompt::Prompt> install_prompt) {
  ASSERT_TRUE(install_prompt.get());
  EXPECT_EQ(regular_permissions_count,
            install_prompt->GetPermissionCount(
                ExtensionInstallPrompt::REGULAR_PERMISSIONS));
  EXPECT_EQ(withheld_permissions_count,
            install_prompt->GetPermissionCount(
                ExtensionInstallPrompt::WITHHELD_PERMISSIONS));
  quit_closure.Run();
}

void SetImage(gfx::Image* image_out,
              const base::Closure& quit_closure,
              const gfx::Image& image_in) {
  *image_out = image_in;
  quit_closure.Run();
}

class ExtensionInstallPromptUnitTest : public testing::Test {
 public:
  ExtensionInstallPromptUnitTest() {}
  ~ExtensionInstallPromptUnitTest() override {}

  // testing::Test:
  void SetUp() override {
    profile_.reset(new TestingProfile());
  }
  void TearDown() override {
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPromptUnitTest);
};

}  // namespace

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsPermissionWarnings) {
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermission::kTab);
  scoped_ptr<const PermissionSet> permission_set(
      new PermissionSet(api_permissions, ManifestPermissionSet(),
                        URLPatternSet(), URLPatternSet()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(DictionaryBuilder()
                                     .Set("name", "foo")
                                     .Set("version", "1.0")
                                     .Set("manifest_version", 2)
                                     .Set("description", "Random Ext")))
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;
  prompt.ShowDialog(
      nullptr,  // no delegate
      extension.get(), nullptr,
      make_scoped_ptr(new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT)),
      permission_set.Pass(),
      base::Bind(&VerifyPromptPermissionsCallback, run_loop.QuitClosure(),
                 1u,    // |regular_permissions_count|.
                 0u));  // |withheld_permissions_count|.
  run_loop.Run();
}

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsWithheldPermissions) {
  // Enable consent flag so that <all_hosts> permissions get withheld.
  FeatureSwitch::ScopedOverride enable_scripts_switch(
      FeatureSwitch::scripts_require_action(), true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              std::move(DictionaryBuilder()
                            .Set("name", "foo")
                            .Set("version", "1.0")
                            .Set("manifest_version", 2)
                            .Set("description", "Random Ext")
                            .Set("permissions",
                                 std::move(ListBuilder()
                                               .Append("http://*/*")
                                               .Append("http://www.google.com/")
                                               .Append("tabs")))))
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;

  // We expect <all_hosts> to be withheld, but http://www.google.com/ and tabs
  // permissions should be granted as regular permissions.
  prompt.ShowDialog(
      nullptr, extension.get(), nullptr,
      base::Bind(&VerifyPromptPermissionsCallback, run_loop.QuitClosure(),
                 2u,    // |regular_permissions_count|.
                 1u));  // |withheld_permissions_count|.
  run_loop.Run();
}

TEST_F(ExtensionInstallPromptUnitTest,
       DelegatedPromptShowsOptionalPermissions) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(
              DictionaryBuilder()
                  .Set("name", "foo")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("description", "Random Ext")
                  .Set("permissions",
                       std::move(ListBuilder().Append("clipboardRead")))
                  .Set("optional_permissions",
                       std::move(ListBuilder().Append("tabs")))))
          .Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;

  scoped_ptr<ExtensionInstallPrompt::Prompt> sub_prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::DELEGATED_PERMISSIONS_PROMPT));
  sub_prompt->set_delegated_username("Username");
  prompt.ShowDialog(
      nullptr,  // no delegate
      extension.get(), nullptr, sub_prompt.Pass(),
      base::Bind(&VerifyPromptPermissionsCallback, run_loop.QuitClosure(),
                 2u,    // |regular_permissions_count|.
                 0u));  // |withheld_permissions_count|.
  run_loop.Run();
}

using ExtensionInstallPromptTestWithService = ExtensionServiceTestWithInstall;

TEST_F(ExtensionInstallPromptTestWithService, ExtensionInstallPromptIconsTest) {
  InitializeEmptyExtensionService();

  const Extension* extension = PackAndInstallCRX(
      data_dir().AppendASCII("simple_with_icon"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  std::vector<ImageLoader::ImageRepresentation> image_rep(
      1, ImageLoader::ImageRepresentation(
             IconsInfo::GetIconResource(extension,
                                        extension_misc::EXTENSION_ICON_LARGE,
                                        ExtensionIconSet::MATCH_BIGGER),
             ImageLoader::ImageRepresentation::NEVER_RESIZE, gfx::Size(),
             ui::SCALE_FACTOR_100P));
  base::RunLoop image_loop;
  gfx::Image image;
  ImageLoader::Get(browser_context())
      ->LoadImagesAsync(
          extension, image_rep,
          base::Bind(&SetImage, &image, image_loop.QuitClosure()));
  image_loop.Run();
  ASSERT_FALSE(image.IsEmpty());
  content::TestWebContentsFactory factory;
  content::WebContents* web_contents =
      factory.CreateWebContents(browser_context());
  {
    ExtensionInstallPrompt prompt(web_contents);
    base::RunLoop run_loop;
    prompt.ShowDialog(nullptr,  // No delegate.
                      extension,
                      nullptr,  // Force an icon fetch.
                      base::Bind(&VerifyPromptIconCallback,
                                 run_loop.QuitClosure(), image.AsBitmap()));
    run_loop.Run();
  }

  {
    ExtensionInstallPrompt prompt(web_contents);
    base::RunLoop run_loop;
    gfx::ImageSkia app_icon = util::GetDefaultAppIcon();
    prompt.ShowDialog(nullptr,  // No delegate.
                      extension,
                      app_icon.bitmap(),  // Use a different icon.
                      base::Bind(&VerifyPromptIconCallback,
                                 run_loop.QuitClosure(), *app_icon.bitmap()));
    run_loop.Run();
  }
}

}  // namespace extensions
