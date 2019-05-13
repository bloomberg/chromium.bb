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
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
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
const GURL kAppScope("https://www.chromium.org/");
const char kAppAlternativeScope[] = "http://www.chromium.org/new/";
const char kAppTitle[] = "Test title";
const char kAlternativeAppTitle[] = "Different test title";
const char kAppDescription[] = "Test description";

const int kIconSizeTiny = extension_misc::EXTENSION_ICON_BITTY;
const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
const int kIconSizeMedium = extension_misc::EXTENSION_ICON_MEDIUM;
const int kIconSizeLarge = extension_misc::EXTENSION_ICON_LARGE;

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

    auto test_url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
    test_url_loader_ = test_url_loader.get();
    install_manager().SetUrlLoaderForTesting(std::move(test_url_loader));
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

  web_app::WebAppInstallManager& install_manager() {
    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
    return *static_cast<web_app::WebAppInstallManager*>(
        &provider->install_manager());
  }

  web_app::TestWebAppUrlLoader& url_loader() {
    DCHECK(test_url_loader_);
    return *test_url_loader_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  web_app::TestWebAppUrlLoader* test_url_loader_ = nullptr;

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

TEST_F(InstallManagerBookmarkAppTest, InstallOrUpdateWebAppFromSync) {
  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = GURL(kAppUrl);
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  web_app_info->scope = GURL(kAppScope);
  web_app_info->icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED));

  auto web_app_info2 = std::make_unique<WebApplicationInfo>(*web_app_info);
  web_app_info2->title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info2->icons[0] =
      CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED);
  web_app_info2->scope = GURL(kAppAlternativeScope);

  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
  web_app::AppId app_id;

  url_loader().SetNextLoadUrlResult(
      GURL("about:blank"), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  {
    base::RunLoop run_loop;

    provider->install_manager().InstallOrUpdateWebAppFromSync(
        app_id, std::move(web_app_info),
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    run_loop.Run();
  }

#if defined(OS_CHROMEOS)
  // On Chrome OS, sync always locally installs an app.
  const bool expect_locally_installed = true;
#else
  const bool expect_locally_installed = false;
#endif

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_EQ(expect_locally_installed,
              BookmarkAppIsLocallyInstalled(profile(), extension));
  }

  url_loader().SetNextLoadUrlResult(
      GURL("about:blank"), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  // On non-ChromeOS platforms is_locally_installed case depends on IsInstalled.
  // On ChromeOS, it always behaves as if app is installed.
  EXPECT_TRUE(provider->registrar().IsInstalled(app_id));

  {
    base::RunLoop run_loop;

    provider->install_manager().InstallOrUpdateWebAppFromSync(
        app_id, std::move(web_app_info2),
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccess, code);
          EXPECT_EQ(app_id, installed_app_id);
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAlternativeAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_EQ(GURL(kAppAlternativeScope),
              GetScopeURLFromBookmarkApp(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeLarge, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_TRUE(BookmarkAppIsLocallyInstalled(profile(), extension));
  }
}

// TODO(loyso): Convert more tests from bookmark_app_helper_unittest.cc

}  // namespace extensions
