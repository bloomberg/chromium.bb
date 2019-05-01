// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const GURL kAppUrl("https://www.chromium.org/index.html");
const char kAppTitle[] = "Test title";
const char kAppDescription[] = "Test description";
const GURL kAppScope("https://www.chromium.org/");

const int kIconSizeTiny = extension_misc::EXTENSION_ICON_BITTY;
const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
const int kIconSizeMedium = extension_misc::EXTENSION_ICON_MEDIUM;

SkBitmap CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

WebApplicationInfo::IconInfo CreateIconInfoWithBitmap(int size, SkColor color) {
  WebApplicationInfo::IconInfo icon_info;
  icon_info.width = size;
  icon_info.height = size;
  icon_info.data = CreateSquareBitmapWithColor(size, color);
  return icon_info;
}

}  // namespace

class InstallManagerBookmarkAppTest : public ExtensionServiceTestBase {
 public:
  InstallManagerBookmarkAppTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsUnifiedInstall}, {});
  }

  ~InstallManagerBookmarkAppTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service_->Init();
    EXPECT_EQ(0u, registry()->enabled_extensions().size());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  }

  void TearDown() override {
    ExtensionServiceTestBase::TearDown();
    for (content::RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      if (Profile::FromBrowserContext(host->GetBrowserContext()) ==
          profile_.get())
        host->Cleanup();
    }
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(InstallManagerBookmarkAppTest);
};

TEST_F(InstallManagerBookmarkAppTest, CreateWebAppFromInfo) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = kAppUrl;
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  web_app_info->scope = kAppScope;
  web_app_info->icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeTiny, SK_ColorRED));

  base::RunLoop run_loop;
  web_app::AppId app_id;

  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());

  provider->install_manager().InstallWebAppFromInfo(
      std::move(web_app_info), /*no_network_install=*/false,
      WebappInstallSource::ARC,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
        app_id = installed_app_id;
        run_loop.Quit();
      }));

  run_loop.Run();

  const Extension* extension = service_->GetInstalledExtension(app_id);
  EXPECT_TRUE(extension);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(kAppUrl, AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(kAppScope, GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeTiny,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeSmall,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeSmall * 2,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeMedium,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeMedium * 2,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
}

// TODO(loyso): Convert more tests from bookmark_app_helper_unittest.cc

}  // namespace extensions
