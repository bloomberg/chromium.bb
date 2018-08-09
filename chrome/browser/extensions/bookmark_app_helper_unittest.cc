// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace extensions {

namespace {

using ForInstallableSite = BookmarkAppHelper::ForInstallableSite;

const char kManifestUrl[] = "http://www.chromium.org/manifest.json";
const char kAppUrl[] = "http://www.chromium.org/index.html";
const char kAlternativeAppUrl[] = "http://www.notchromium.org";
const char kAppScope[] = "http://www.chromium.org/scope/";
const char kAppAlternativeScope[] = "http://www.chromium.org/new/";
const char kAppDefaultScope[] = "http://www.chromium.org/";
const char kAppTitle[] = "Test title";
const char kAppShortName[] = "Test short name";
const char kAlternativeAppTitle[] = "Different test title";
const char kAppDescription[] = "Test description";
const char kAppIcon1[] = "fav1.png";
const char kAppIcon2[] = "fav2.png";
const char kAppIcon3[] = "fav3.png";
const char kAppIconURL1[] = "http://foo.com/1.png";
const char kAppIconURL2[] = "http://foo.com/2.png";

const int kIconSizeTiny = extension_misc::EXTENSION_ICON_BITTY;
const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
const int kIconSizeMedium = extension_misc::EXTENSION_ICON_MEDIUM;
const int kIconSizeLarge = extension_misc::EXTENSION_ICON_LARGE;

class BookmarkAppHelperTest : public testing::Test {
 public:
  BookmarkAppHelperTest() {}
  ~BookmarkAppHelperTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperTest);
};

class BookmarkAppHelperExtensionServiceTest
    : public extensions::ExtensionServiceTestBase {
 public:
  BookmarkAppHelperExtensionServiceTest() {}
  ~BookmarkAppHelperExtensionServiceTest() override {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service_->Init();
    EXPECT_EQ(0u, registry()->enabled_extensions().size());
  }

  void TearDown() override {
    ExtensionServiceTestBase::TearDown();
    for (content::RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd();
         i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      if (Profile::FromBrowserContext(host->GetBrowserContext()) ==
          profile_.get())
        host->Cleanup();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperExtensionServiceTest);
};

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

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(ExtensionService* service,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents)
      : BookmarkAppHelper(service->profile(),
                          web_app_info,
                          contents,
                          WebappInstallSource::MENU_BROWSER_TAB),
        bitmap_(CreateSquareBitmapWithColor(32, SK_ColorRED)) {}

  ~TestBookmarkAppHelper() override {}

  void CreationComplete(const extensions::Extension* extension,
                        const WebApplicationInfo& web_app_info) {
    extension_ = extension;
  }

  void CompleteInstallableCheck(const char* manifest_url,
                                const blink::Manifest& manifest,
                                ForInstallableSite for_installable_site) {
    bool installable = for_installable_site == ForInstallableSite::kYes;
    InstallableData data = {
        installable ? NO_ERROR_DETECTED : MANIFEST_DISPLAY_NOT_SUPPORTED,
        GURL(manifest_url),
        &manifest,
        GURL(kAppIconURL1),
        &bitmap_,
        GURL(),
        nullptr,
        installable,
        installable,
    };
    BookmarkAppHelper::OnDidPerformInstallableCheck(data);
  }

  void CompleteIconDownload(
      bool success,
      const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

  const Extension* extension() { return extension_; }

  const WebAppIconDownloader* web_app_icon_downloader() {
    return web_app_icon_downloader_.get();
  }

 private:
  const Extension* extension_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

}  // namespace

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkApp) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  helper.CompleteInstallableCheck(kManifestUrl, blink::Manifest(),
                                  ForInstallableSite::kNo);

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  icon_map[GURL(kAppUrl)].push_back(
      CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
  helper.CompleteIconDownload(true, icon_map);

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      AppBannerSettingsHelper::GetSingleBannerEvent(
          contents.get(), web_app_info.app_url, web_app_info.app_url.spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN)
          .is_null());
}

class BookmarkAppHelperExtensionServiceInstallableSiteTest
    : public BookmarkAppHelperExtensionServiceTest,
      public ::testing::WithParamInterface<ForInstallableSite> {
 public:
  BookmarkAppHelperExtensionServiceInstallableSiteTest() {}
  ~BookmarkAppHelperExtensionServiceInstallableSiteTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      BookmarkAppHelperExtensionServiceInstallableSiteTest);
};

TEST_P(BookmarkAppHelperExtensionServiceInstallableSiteTest,
       CreateBookmarkAppWithManifest) {
  WebApplicationInfo web_app_info;

  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  manifest.theme_color = SK_ColorBLUE;
  helper.CompleteInstallableCheck(kManifestUrl, manifest, GetParam());

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  helper.CompleteIconDownload(true, icon_map);

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(SK_ColorBLUE, AppThemeColorInfo::GetThemeColor(extension).value());
  EXPECT_FALSE(
      AppBannerSettingsHelper::GetSingleBannerEvent(
          contents.get(), manifest.start_url, manifest.start_url.spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN)
          .is_null());

  if (GetParam() == ForInstallableSite::kYes) {
    EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
  } else {
    EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  }
}

TEST_P(BookmarkAppHelperExtensionServiceInstallableSiteTest,
       CreateBookmarkAppWithManifestIcons) {
  WebApplicationInfo web_app_info;
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  blink::Manifest::ImageResource icon;
  icon.src = GURL(kAppIconURL1);
  manifest.icons.push_back(icon);
  icon.src = GURL(kAppIconURL2);
  manifest.icons.push_back(icon);
  helper.CompleteInstallableCheck(kManifestUrl, manifest, GetParam());

  // Favicon URLs are ignored because the site has a manifest with icons.
  EXPECT_FALSE(helper.web_app_icon_downloader()->need_favicon_urls_);

  // Only 1 icon should be downloading since the other was provided by the
  // InstallableManager.
  EXPECT_EQ(1u, helper.web_app_icon_downloader()->in_progress_requests_.size());

  std::map<GURL, std::vector<SkBitmap>> icon_map;
  icon_map[GURL(kAppIconURL2)].push_back(
      CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
  helper.CompleteIconDownload(true, icon_map);

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));

  if (GetParam() == ForInstallableSite::kYes) {
    EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
  } else {
    EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  }
}

TEST_P(BookmarkAppHelperExtensionServiceInstallableSiteTest,
       CreateBookmarkAppWithManifestNoScope) {
  WebApplicationInfo web_app_info;

  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  helper.CompleteInstallableCheck(kManifestUrl, manifest, GetParam());

  std::map<GURL, std::vector<SkBitmap>> icon_map;
  helper.CompleteIconDownload(true, icon_map);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());

  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  if (GetParam() == ForInstallableSite::kYes) {
    EXPECT_EQ(GURL(kAppDefaultScope), GetScopeURLFromBookmarkApp(extension));
  } else {
    EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  }
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        BookmarkAppHelperExtensionServiceInstallableSiteTest,
                        ::testing::Values(ForInstallableSite::kNo,
                                          ForInstallableSite::kYes));

TEST_F(BookmarkAppHelperExtensionServiceTest,
       CreateBookmarkAppDefaultLauncherContainers) {
  auto scoped_feature_list = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list->InitAndEnableFeature(features::kDesktopPWAWindowing);

  WebApplicationInfo web_app_info;
  std::map<GURL, std::vector<SkBitmap>> icon_map;

  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  manifest.scope = GURL(kAppScope);
  {
    TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
    helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                             base::Unretained(&helper)));

    helper.CompleteInstallableCheck(kManifestUrl, manifest,
                                    ForInstallableSite::kYes);
    helper.CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
    EXPECT_TRUE(helper.extension());
    const Extension* extension =
        service_->GetInstalledExtension(helper.extension()->id());
    EXPECT_TRUE(extension);
    EXPECT_EQ(LAUNCH_CONTAINER_WINDOW,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
  {
    TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
    helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                             base::Unretained(&helper)));

    helper.CompleteInstallableCheck(kManifestUrl, manifest,
                                    ForInstallableSite::kNo);
    helper.CompleteIconDownload(true, icon_map);

    content::RunAllTasksUntilIdle();
    EXPECT_TRUE(helper.extension());
    const Extension* extension =
        service_->GetInstalledExtension(helper.extension()->id());
    EXPECT_TRUE(extension);
    EXPECT_EQ(LAUNCH_CONTAINER_TAB,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
}

TEST_F(BookmarkAppHelperExtensionServiceTest,
       CreateBookmarkAppWithoutManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.app_url = GURL(kAppUrl);

  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  helper.CompleteInstallableCheck(kManifestUrl, blink::Manifest(),
                                  ForInstallableSite::kNo);
  std::map<GURL, std::vector<SkBitmap>> icon_map;
  helper.CompleteIconDownload(true, icon_map);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());

  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(AppThemeColorInfo::GetThemeColor(extension));
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppNoContents) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.scope = GURL(kAppScope);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeTiny, SK_ColorRED));

  TestBookmarkAppHelper helper(service_, web_app_info, NULL);
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(GURL(kAppScope), GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension, kIconSizeTiny,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension,
                                 kIconSizeSmall * 2,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeMedium, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension,
                                 kIconSizeMedium * 2,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateAndUpdateBookmarkApp) {
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.scope = GURL(kAppScope);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED));

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  content::RunAllTasksUntilIdle();

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
  }

  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.icons[0] = CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED);
  web_app_info.scope = GURL(kAppAlternativeScope);

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  content::RunAllTasksUntilIdle();

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
  }
}

TEST_F(BookmarkAppHelperTest, UpdateWebAppInfoFromManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.app_url = GURL(kAlternativeAppUrl);
  WebApplicationInfo::IconInfo info;
  info.url = GURL(kAppIcon1);
  web_app_info.icons.push_back(info);

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.short_name = base::NullableString16(base::UTF8ToUTF16(kAppShortName),
                                               false);

  BookmarkAppHelper::UpdateWebAppInfoFromManifest(manifest, &web_app_info,
                                                  ForInstallableSite::kNo);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(GURL(kAppUrl), web_app_info.app_url);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon1), web_app_info.icons[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);

  blink::Manifest::ImageResource icon;
  icon.src = GURL(kAppIcon2);
  manifest.icons.push_back(icon);
  icon.src = GURL(kAppIcon3);
  manifest.icons.push_back(icon);

  BookmarkAppHelper::UpdateWebAppInfoFromManifest(
      manifest, &web_app_info, BookmarkAppHelper::ForInstallableSite::kNo);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);

  EXPECT_EQ(2u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon2), web_app_info.icons[0].url);
  EXPECT_EQ(GURL(kAppIcon3), web_app_info.icons[1].url);
}

// Tests "scope" is only set for installable sites.
TEST_F(BookmarkAppHelperTest, UpdateWebAppInfoFromManifestInstallableSite) {
  {
    blink::Manifest manifest;
    manifest.start_url = GURL(kAppUrl);
    WebApplicationInfo web_app_info;
    BookmarkAppHelper::UpdateWebAppInfoFromManifest(
        manifest, &web_app_info,
        BookmarkAppHelper::ForInstallableSite::kUnknown);
    EXPECT_EQ(GURL(), web_app_info.scope);
  }

  {
    blink::Manifest manifest;
    manifest.start_url = GURL(kAppUrl);
    WebApplicationInfo web_app_info;
    BookmarkAppHelper::UpdateWebAppInfoFromManifest(
        manifest, &web_app_info, BookmarkAppHelper::ForInstallableSite::kNo);
    EXPECT_EQ(GURL(), web_app_info.scope);
  }

  {
    blink::Manifest manifest;
    manifest.start_url = GURL(kAppUrl);
    WebApplicationInfo web_app_info;
    BookmarkAppHelper::UpdateWebAppInfoFromManifest(
        manifest, &web_app_info, BookmarkAppHelper::ForInstallableSite::kYes);

    EXPECT_NE(GURL(), web_app_info.scope);
  }
}

TEST_F(BookmarkAppHelperTest, IsValidBookmarkAppUrl) {
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("https://chromium.org")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("https://www.chromium.org")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(
      GURL("https://www.chromium.org/path/to/page.html")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("http://chromium.org")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("http://www.chromium.org")));
  EXPECT_TRUE(
      IsValidBookmarkAppUrl(GURL("http://www.chromium.org/path/to/page.html")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(
      GURL("chrome-extension://oafaagfgbdpldilgjjfjocjglfbolmac")));

  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("ftp://www.chromium.org")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("chrome://flags")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("about:blank")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(
      GURL("file://mhjfbmdgcfjbbpaeojofohoefgiehjai")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("chrome://extensions")));
}

}  // namespace extensions
