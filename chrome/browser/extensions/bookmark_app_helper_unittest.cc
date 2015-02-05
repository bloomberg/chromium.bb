// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace {

const char kAppUrl[] = "http://www.chromium.org";
const char kAlternativeAppUrl[] = "http://www.notchromium.org";
const char kAppTitle[] = "Test title";
const char kAppShortName[] = "Test short name";
const char kAlternativeAppTitle[] = "Different test title";
const char kAppDescription[] = "Test description";
const char kAppIcon1[] = "fav1.png";
const char kAppIcon2[] = "fav2.png";
const char kAppIcon3[] = "fav3.png";

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

void ValidateWebApplicationInfo(base::Closure callback,
                                const WebApplicationInfo& expected,
                                const WebApplicationInfo& actual) {
  EXPECT_EQ(expected.title, actual.title);
  EXPECT_EQ(expected.description, actual.description);
  EXPECT_EQ(expected.app_url, actual.app_url);
  EXPECT_EQ(expected.icons.size(), actual.icons.size());
  for (size_t i = 0; i < expected.icons.size(); ++i) {
    EXPECT_EQ(expected.icons[i].width, actual.icons[i].width);
    EXPECT_EQ(expected.icons[i].height, actual.icons[i].height);
    EXPECT_EQ(expected.icons[i].url, actual.icons[i].url);
    EXPECT_TRUE(
        gfx::BitmapsAreEqual(expected.icons[i].data, actual.icons[i].data));
  }
  callback.Run();
}

}  // namespace

namespace extensions {

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(ExtensionService* service,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents)
      : BookmarkAppHelper(service->profile(), web_app_info, contents) {}

  ~TestBookmarkAppHelper() override {}

  void CreationComplete(const extensions::Extension* extension,
                        const WebApplicationInfo& web_app_info) {
    extension_ = extension;
  }

  void CompleteGetManifest(const content::Manifest& manifest) {
    BookmarkAppHelper::OnDidGetManifest(manifest);
  }

  void CompleteIconDownload(
      bool success,
      const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

  const Extension* extension() { return extension_; }

 private:
  const Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkApp) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  scoped_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  icon_map[GURL(kAppUrl)].push_back(
      CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
  helper.CompleteIconDownload(true, icon_map);

  base::RunLoop().RunUntilIdle();
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
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppWithManifest) {
  WebApplicationInfo web_app_info;

  scoped_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  content::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  helper.CompleteGetManifest(manifest);

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  helper.CompleteIconDownload(true, icon_map);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppNoContents) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeTiny, SK_ColorRED));

  TestBookmarkAppHelper helper(service_, web_app_info, NULL);
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  // The tiny icon should have been removed and only the generated ones used.
  EXPECT_TRUE(
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
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED));

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  base::RunLoop().RunUntilIdle();

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
  }

  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.icons[0] = CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED);

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  base::RunLoop().RunUntilIdle();

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAlternativeAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_TRUE(extensions::IconsInfo::GetIconResource(
                    extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                    .empty());
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeLarge, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
  }
}

TEST_F(BookmarkAppHelperExtensionServiceTest, GetWebApplicationInfo) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED));
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED));

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  base::RunLoop run_loop;
  extensions::GetWebApplicationInfoFromApp(
      profile_.get(), registry()->enabled_extensions().begin()->get(),
      base::Bind(&ValidateWebApplicationInfo, run_loop.QuitClosure(),
                 web_app_info));
  run_loop.Run();
}

TEST_F(BookmarkAppHelperTest, UpdateWebAppInfoFromManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.app_url = GURL(kAlternativeAppUrl);
  WebApplicationInfo::IconInfo info;
  info.url = GURL(kAppIcon1);
  web_app_info.icons.push_back(info);

  content::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.short_name = base::NullableString16(base::UTF8ToUTF16(kAppShortName),
                                               false);

  BookmarkAppHelper::UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(GURL(kAppUrl), web_app_info.app_url);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon1), web_app_info.icons[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);

  content::Manifest::Icon icon;
  icon.src = GURL(kAppIcon2);
  manifest.icons.push_back(icon);
  icon.src = GURL(kAppIcon3);
  manifest.icons.push_back(icon);

  BookmarkAppHelper::UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);

  EXPECT_EQ(2u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon2), web_app_info.icons[0].url);
  EXPECT_EQ(GURL(kAppIcon3), web_app_info.icons[1].url);
}

TEST_F(BookmarkAppHelperTest, IsValidBookmarkAppUrl) {
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("https://www.chromium.org")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("http://www.chromium.org/path")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("ftp://www.chromium.org")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("chrome://flags")));
}

}  // namespace extensions
