// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/convert_web_app.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/web_apps.h"
#include "extensions/common/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "webkit/glue/image_decoder.h"

namespace extensions {

namespace {

// Returns an icon info corresponding to a canned icon.
WebApplicationInfo::IconInfo GetIconInfo(const GURL& url, int size) {
  WebApplicationInfo::IconInfo result;

  base::FilePath icon_file;
  if (!PathService::Get(chrome::DIR_TEST_DATA, &icon_file)) {
    ADD_FAILURE() << "Could not get test data directory.";
    return result;
  }

  icon_file = icon_file.AppendASCII("extensions")
                       .AppendASCII("convert_web_app")
                       .AppendASCII(StringPrintf("%i.png", size));

  result.url = url;
  result.width = size;
  result.height = size;

  std::string icon_data;
  if (!file_util::ReadFileToString(icon_file, &icon_data)) {
    ADD_FAILURE() << "Could not read test icon.";
    return result;
  }

  webkit_glue::ImageDecoder decoder;
  result.data = decoder.Decode(
      reinterpret_cast<const unsigned char*>(icon_data.c_str()),
      icon_data.size());
  EXPECT_FALSE(result.data.isNull()) << "Could not decode test icon.";

  return result;
}

base::Time GetTestTime(int year, int month, int day, int hour, int minute,
                       int second, int millisecond) {
  base::Time::Exploded exploded = {0};
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_month = day;
  exploded.hour = hour;
  exploded.minute = minute;
  exploded.second = second;
  exploded.millisecond = millisecond;
  return base::Time::FromUTCExploded(exploded);
}

}  // namespace


TEST(ExtensionFromWebApp, GenerateVersion) {
  EXPECT_EQ("2010.1.1.0",
            ConvertTimeToExtensionVersion(
                GetTestTime(2010, 1, 1, 0, 0, 0, 0)));
  EXPECT_EQ("2010.12.31.22111",
            ConvertTimeToExtensionVersion(
                GetTestTime(2010, 12, 31, 8, 5, 50, 500)));
  EXPECT_EQ("2010.10.1.65535",
            ConvertTimeToExtensionVersion(
                GetTestTime(2010, 10, 1, 23, 59, 59, 999)));
}

TEST(ExtensionFromWebApp, Basic) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  WebApplicationInfo web_app;
  web_app.manifest_url = GURL("http://aaronboodman.com/gearpad/manifest.json");
  web_app.title = ASCIIToUTF16("Gearpad");
  web_app.description = ASCIIToUTF16("The best text editor in the universe!");
  web_app.app_url = GURL("http://aaronboodman.com/gearpad/");
  web_app.permissions.push_back("geolocation");
  web_app.permissions.push_back("notifications");
  web_app.urls.push_back(GURL("http://aaronboodman.com/gearpad/"));

  const int sizes[] = {16, 48, 128};
  for (size_t i = 0; i < arraysize(sizes); ++i) {
    GURL icon_url(web_app.app_url.Resolve(StringPrintf("%i.png", sizes[i])));
    web_app.icons.push_back(GetIconInfo(icon_url, sizes[i]));
  }

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0),
      extensions_dir.path());
  ASSERT_TRUE(extension.get());

  base::ScopedTempDir extension_dir;
  EXPECT_TRUE(extension_dir.Set(extension->path()));

  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_FALSE(extension->is_legacy_packaged_app());

  EXPECT_EQ("lJqm1+jncOHClAuwif1QxNJKfeV9Fbl9IBZx7FkNwkA=",
            extension->public_key());
  EXPECT_EQ("ncnbaadanljoanockmphfdkimpdedemj", extension->id());
  EXPECT_EQ("1978.12.11.0", extension->version()->GetString());
  EXPECT_EQ(UTF16ToUTF8(web_app.title), extension->name());
  EXPECT_EQ(UTF16ToUTF8(web_app.description), extension->description());
  EXPECT_EQ(web_app.app_url, extension->GetFullLaunchURL());
  EXPECT_EQ(2u, extension->GetActivePermissions()->apis().size());
  EXPECT_TRUE(extension->HasAPIPermission("geolocation"));
  EXPECT_TRUE(extension->HasAPIPermission("notifications"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("http://aaronboodman.com/gearpad/*",
            extension->web_extent().patterns().begin()->GetAsString());

  EXPECT_EQ(web_app.icons.size(), IconsInfo::GetIcons(extension).map().size());
  for (size_t i = 0; i < web_app.icons.size(); ++i) {
    EXPECT_EQ(StringPrintf("icons/%i.png", web_app.icons[i].width),
              IconsInfo::GetIcons(extension).Get(
                  web_app.icons[i].width, ExtensionIconSet::MATCH_EXACTLY));
    ExtensionResource resource = IconsInfo::GetIconResource(
        extension, web_app.icons[i].width, ExtensionIconSet::MATCH_EXACTLY);
    ASSERT_TRUE(!resource.empty());
    EXPECT_TRUE(file_util::PathExists(resource.GetFilePath()));
  }
}

TEST(ExtensionFromWebApp, Minimal) {
  base::ScopedTempDir extensions_dir;
  ASSERT_TRUE(extensions_dir.CreateUniqueTempDir());

  WebApplicationInfo web_app;
  web_app.manifest_url = GURL("http://aaronboodman.com/gearpad/manifest.json");
  web_app.title = ASCIIToUTF16("Gearpad");
  web_app.app_url = GURL("http://aaronboodman.com/gearpad/");

  scoped_refptr<Extension> extension = ConvertWebAppToExtension(
      web_app, GetTestTime(1978, 12, 11, 0, 0, 0, 0),
      extensions_dir.path());
  ASSERT_TRUE(extension.get());

  base::ScopedTempDir extension_dir;
  EXPECT_TRUE(extension_dir.Set(extension->path()));

  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_FALSE(extension->is_legacy_packaged_app());

  EXPECT_EQ("lJqm1+jncOHClAuwif1QxNJKfeV9Fbl9IBZx7FkNwkA=",
            extension->public_key());
  EXPECT_EQ("ncnbaadanljoanockmphfdkimpdedemj", extension->id());
  EXPECT_EQ("1978.12.11.0", extension->version()->GetString());
  EXPECT_EQ(UTF16ToUTF8(web_app.title), extension->name());
  EXPECT_EQ("", extension->description());
  EXPECT_EQ(web_app.app_url, extension->GetFullLaunchURL());
  EXPECT_EQ(0u, IconsInfo::GetIcons(extension).map().size());
  EXPECT_EQ(0u, extension->GetActivePermissions()->apis().size());
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://aaronboodman.com/*",
            extension->web_extent().patterns().begin()->GetAsString());
}

}  // namespace extensions
