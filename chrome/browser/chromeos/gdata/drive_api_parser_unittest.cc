// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

#define IF_EXPECT_EQ(arg1, arg2) \
  EXPECT_EQ(arg1, arg2); \
  if (arg1 == arg2)

#define IF_EXPECT_TRUE(arg) \
  EXPECT_TRUE(arg); \
  if (arg)

namespace gdata {

class DriveAPIParserTest : public testing::Test {
 protected:
  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path;
    std::string error;
    // Test files for this unit test are located in
    // src/chrome/test/data/chromeos/drive/*
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("drive")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();

    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(NULL, &error);
    EXPECT_TRUE(value) << "Parse error " << path.value() << ": " << error;
    return value;
  }
};

// Test about resource parsing.
TEST_F(DriveAPIParserTest, AboutResourceParser) {
  std::string error;
  scoped_ptr<Value> document(LoadJSONFile("about.json"));
  ASSERT_TRUE(document.get());

  ASSERT_EQ(Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<AboutResource> resource(new AboutResource());
  EXPECT_TRUE(resource->Parse(*document));

  EXPECT_EQ("0AIv7G8yEYAWHUk9123", resource->root_folder_id());
  EXPECT_EQ(5368709120LL, resource->quota_bytes_total());
  EXPECT_EQ(1073741824LL, resource->quota_bytes_used());
  EXPECT_EQ(8177LL, resource->largest_change_id());
}

// Test app list parsing.
TEST_F(DriveAPIParserTest, AppListParser) {
  std::string error;
  scoped_ptr<Value> document(LoadJSONFile("applist.json"));
  ASSERT_TRUE(document.get());

  ASSERT_EQ(Value::TYPE_DICTIONARY, document->GetType());
  scoped_ptr<AppList> applist(new AppList);
  EXPECT_TRUE(applist->Parse(*document));

  EXPECT_EQ("\"Jm4BaSnCWNND-noZsHINRqj4ABC/tuqRBw0lvjUdPtc_2msA1tN4XYZ\"",
            applist->etag());
  IF_EXPECT_EQ(2U, applist->items().size()) {
    // Check Drive app 1
    const AppResource& app1 = *applist->items()[0];
    EXPECT_EQ("123456788192", app1.id());
    EXPECT_EQ("Drive app 1", app1.name());
    EXPECT_EQ("", app1.object_type());
    EXPECT_EQ(true, app1.supports_create());
    EXPECT_EQ(true, app1.supports_import());
    EXPECT_EQ(true, app1.is_installed());
    EXPECT_EQ(false, app1.is_authorized());
    EXPECT_EQ("https://chrome.google.com/webstore/detail/"
              "abcdefghabcdefghabcdefghabcdefgh",
              app1.product_url().spec());

    IF_EXPECT_EQ(1U, app1.primary_mimetypes().size()) {
      EXPECT_EQ("application/vnd.google-apps.drive-sdk.123456788192",
                *app1.primary_mimetypes()[0]);
    }

    IF_EXPECT_EQ(2U, app1.secondary_mimetypes().size()) {
      EXPECT_EQ("text/html", *app1.secondary_mimetypes()[0]);
      EXPECT_EQ("text/plain", *app1.secondary_mimetypes()[1]);
    }

    IF_EXPECT_EQ(2U, app1.primary_file_extensions().size()) {
      EXPECT_EQ("exe", *app1.primary_file_extensions()[0]);
      EXPECT_EQ("com", *app1.primary_file_extensions()[1]);
    }

    EXPECT_EQ(0U, app1.secondary_file_extensions().size());

    IF_EXPECT_EQ(6U, app1.icons().size()) {
      const DriveAppIcon& icon1 = *app1.icons()[0];
      EXPECT_EQ(DriveAppIcon::APPLICATION, icon1.category());
      EXPECT_EQ(10, icon1.icon_side_length());
      EXPECT_EQ("http://www.example.com/10.png", icon1.icon_url().spec());

      const DriveAppIcon& icon6 = *app1.icons()[5];
      EXPECT_EQ(DriveAppIcon::SHARED_DOCUMENT, icon6.category());
      EXPECT_EQ(16, icon6.icon_side_length());
      EXPECT_EQ("http://www.example.com/ds16.png", icon6.icon_url().spec());
    }

    // Check Drive app 2
    const AppResource& app2 = *applist->items()[1];
    EXPECT_EQ("876543210000", app2.id());
    EXPECT_EQ("Drive app 2", app2.name());
    EXPECT_EQ("", app2.object_type());
    EXPECT_EQ(false, app2.supports_create());
    EXPECT_EQ(false, app2.supports_import());
    EXPECT_EQ(true, app2.is_installed());
    EXPECT_EQ(false, app2.is_authorized());
    EXPECT_EQ("https://chrome.google.com/webstore/detail/"
              "hgfedcbahgfedcbahgfedcbahgfedcba",
              app2.product_url().spec());

    IF_EXPECT_EQ(3U, app2.primary_mimetypes().size()) {
      EXPECT_EQ("image/jpeg",
                *app2.primary_mimetypes()[0]);
      EXPECT_EQ("image/png",
                *app2.primary_mimetypes()[1]);
      EXPECT_EQ("application/vnd.google-apps.drive-sdk.876543210000",
                *app2.primary_mimetypes()[2]);
    }

    EXPECT_EQ(0U, app2.secondary_mimetypes().size());
    EXPECT_EQ(0U, app2.primary_file_extensions().size());
    EXPECT_EQ(0U, app2.secondary_file_extensions().size());

    IF_EXPECT_EQ(3U, app2.icons().size()) {
      const DriveAppIcon& icon2 = *app2.icons()[1];
      EXPECT_EQ(DriveAppIcon::DOCUMENT, icon2.category());
      EXPECT_EQ(10, icon2.icon_side_length());
      EXPECT_EQ("http://www.example.com/d10.png", icon2.icon_url().spec());
    }
  }
}

}  // namespace gdata
