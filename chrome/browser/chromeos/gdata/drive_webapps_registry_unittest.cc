// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace gdata {

class DriveWebAppsRegistryTest : public testing::Test {
 protected:
  DriveWebAppsRegistryTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  bool VerifyApp(const ScopedVector<DriveWebAppInfo>& list,
                 const std::string& web_store_id,
                 const std::string& app_id,
                 const std::string& app_name,
                 const std::string& object_type,
                 bool is_primary) {
    bool found = false;
    for (ScopedVector<DriveWebAppInfo>::const_iterator it = list.begin();
         it != list.end(); ++it) {
      const DriveWebAppInfo* app = *it;
      if (web_store_id == app->web_store_id) {
        EXPECT_EQ(app_id, app->app_id);
        EXPECT_EQ(app_name, UTF16ToUTF8(app->app_name));
        EXPECT_EQ(object_type, UTF16ToUTF8(app->object_type));
        EXPECT_EQ(is_primary, app->is_primary_selector);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Unable to find app with web_store_id "
        << web_store_id;
    return found;
  }

  bool VerifyApp1(const ScopedVector<DriveWebAppInfo>& list,
                  bool is_primary) {
    return VerifyApp(list, "abcdefabcdef", "11111111",
              "Drive App 1", "Drive App Object 1",
              is_primary);
  }

  bool VerifyApp2(const ScopedVector<DriveWebAppInfo>& list,
                  bool is_primary) {
    return VerifyApp(list, "deadbeefdeadbeef", "22222222",
              "Drive App 2", "Drive App Object 2",
              is_primary);
  }

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(DriveWebAppsRegistryTest, LoadAndFindWebApps) {
  scoped_ptr<Value> document(
      test_util::LoadJSONFile("gdata/account_metadata.json"));
  ASSERT_TRUE(document.get());
  ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
  DictionaryValue* entry_value;
  ASSERT_TRUE(reinterpret_cast<DictionaryValue*>(document.get())->GetDictionary(
      std::string("entry"), &entry_value));
  ASSERT_TRUE(entry_value);

  // Load feed.
  scoped_ptr<AccountMetadataFeed> feed(
      AccountMetadataFeed::CreateFrom(*document));
  ASSERT_TRUE(feed.get());
  scoped_ptr<DriveWebAppsRegistry> web_apps(new DriveWebAppsRegistry);
  web_apps->UpdateFromFeed(*feed.get());

  // Find by extension 'ext_1'.
  ScopedVector<DriveWebAppInfo> ext_1_results;
  FilePath ext1_file(FILE_PATH_LITERAL("gdata/SampleFile.ext_1"));
  web_apps->GetWebAppsForFile(ext1_file, std::string(), &ext_1_results);
  ASSERT_EQ(1U, ext_1_results.size());
  EXPECT_TRUE(VerifyApp1(ext_1_results, true));

  // Find by extension 'ext_3'.
  ScopedVector<DriveWebAppInfo> ext_3_results;
  FilePath ext3_file(FILE_PATH_LITERAL("gdata/AnotherFile.ext_3"));
  web_apps->GetWebAppsForFile(ext3_file, std::string(), &ext_3_results);
  ASSERT_EQ(2U, ext_3_results.size());
  EXPECT_TRUE(VerifyApp1(ext_3_results, false));
  EXPECT_TRUE(VerifyApp2(ext_3_results, true));

  // Find by mimetype 'ext_3'.
  ScopedVector<DriveWebAppInfo> mime_results;
  web_apps->GetWebAppsForFile(FilePath(), "application/test_type_2",
                              &mime_results);
  ASSERT_EQ(1U, mime_results.size());
  EXPECT_TRUE(VerifyApp2(mime_results, true));

  // Find by extension and mimetype.
  ScopedVector<DriveWebAppInfo> mime_ext_results;
  FilePath mime_file(FILE_PATH_LITERAL("gdata/MimeFile.ext_2"));
  web_apps->GetWebAppsForFile(mime_file, "application/test_type_2",
                              &mime_ext_results);
  ASSERT_EQ(2U, mime_ext_results.size());
  EXPECT_TRUE(VerifyApp1(mime_ext_results, true));
  EXPECT_TRUE(VerifyApp2(mime_ext_results, true));
}

TEST_F(DriveWebAppsRegistryTest, LoadAndFindDriveWebApps) {
  scoped_ptr<Value> document(test_util::LoadJSONFile("drive/applist.json"));
  ASSERT_TRUE(document.get());
  ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);

  // Load feed.
  scoped_ptr<AppList> app_list(AppList::CreateFrom(*document));
  ASSERT_TRUE(app_list.get());
  scoped_ptr<DriveWebAppsRegistry> web_apps(new DriveWebAppsRegistry);
  web_apps->UpdateFromApplicationList(*app_list.get());

  // Find by primary extension 'exe'.
  ScopedVector<DriveWebAppInfo> ext_results;
  FilePath ext_file(FILE_PATH_LITERAL("drive/file.exe"));
  web_apps->GetWebAppsForFile(ext_file, std::string(), &ext_results);
  ASSERT_EQ(1U, ext_results.size());
  VerifyApp(ext_results, "abcdefghabcdefghabcdefghabcdefgh", "123456788192",
            "Drive app 1", "", true);

  // Find by primary MIME type.
  ScopedVector<DriveWebAppInfo> primary_app;
  web_apps->GetWebAppsForFile(FilePath(),
      "application/vnd.google-apps.drive-sdk.123456788192", &primary_app);
  ASSERT_EQ(1U, primary_app.size());
  VerifyApp(primary_app, "abcdefghabcdefghabcdefghabcdefgh", "123456788192",
            "Drive app 1", "", true);

  // Find by secondary MIME type.
  ScopedVector<DriveWebAppInfo> secondary_app;
  web_apps->GetWebAppsForFile(FilePath(), "text/html", &secondary_app);
  ASSERT_EQ(1U, secondary_app.size());
  VerifyApp(secondary_app, "abcdefghabcdefghabcdefghabcdefgh", "123456788192",
            "Drive app 1", "", false);
}

}  // namespace gdata
