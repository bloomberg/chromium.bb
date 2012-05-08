// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/common/chrome_paths.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace gdata {

class DriveWebAppsRegistryTest : public testing::Test {
 protected:
  DriveWebAppsRegistryTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_){
  }

  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path;
    std::string error;
    // Test files for this unit test are located in
    // src/chrome/test/data/chromeos/gdata/*
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("gdata")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();

    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(NULL, &error);
    EXPECT_TRUE(value) <<
        "Parse error " << path.value() << ": " << error;
    return value;
  }

  void VerifyApp(const ScopedVector<DriveWebAppInfo>& list,
                 const std::string& app_id,
                 const std::string& app_name,
                 const std::string& object_type,
                 bool is_primary) {
    bool found = false;
    for (ScopedVector<DriveWebAppInfo>::const_iterator it = list.begin();
         it != list.end(); ++it) {
      const DriveWebAppInfo* app = *it;
      if (app_id == app->app_id) {
        EXPECT_EQ(app_name, UTF16ToUTF8(app->app_name));
        EXPECT_EQ(object_type, UTF16ToUTF8(app->object_type));
        EXPECT_EQ(is_primary, app->is_primary_selector);
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found) << "WebApp for not found for id " << app_id;
  }

  void VerifyApp1(const ScopedVector<DriveWebAppInfo>& list,
                  bool is_primary) {
    VerifyApp(list, "11111111", "Drive App 1", "Drive App Object 1",
              is_primary);
  }

  void VerifyApp2(const ScopedVector<DriveWebAppInfo>& list,
                  bool is_primary) {
    VerifyApp(list, "22222222", "Drive App 2", "Drive App Object 2",
              is_primary);
  }

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(DriveWebAppsRegistryTest, LoadAndFindWebApps) {
  scoped_ptr<Value> document(LoadJSONFile("account_metadata.json"));
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
  web_apps->UpdateFromFeed(feed.get());

  // Find by extension 'ext_1'.
  ScopedVector<DriveWebAppInfo> ext_1_results;
  FilePath ext1_file(FILE_PATH_LITERAL("gdata/SampleFile.ext_1"));
  web_apps->GetWebAppsForFile(ext1_file, std::string(), &ext_1_results);
  ASSERT_EQ(1U, ext_1_results.size());
  VerifyApp1(ext_1_results, true);

  // Find by extension 'ext_3'.
  ScopedVector<DriveWebAppInfo> ext_3_results;
  FilePath ext3_file(FILE_PATH_LITERAL("gdata/AnotherFile.ext_3"));
  web_apps->GetWebAppsForFile(ext3_file, std::string(), &ext_3_results);
  ASSERT_EQ(2U, ext_3_results.size());
  VerifyApp1(ext_3_results, false);
  VerifyApp2(ext_3_results, true);

  // Find by mimetype 'ext_3'.
  ScopedVector<DriveWebAppInfo> mime_results;
  web_apps->GetWebAppsForFile(FilePath(), "application/test_type_2",
                              &mime_results);
  ASSERT_EQ(1U, mime_results.size());
  VerifyApp2(mime_results, true);

  // Find by extension and mimetype.
  ScopedVector<DriveWebAppInfo> mime_ext_results;
  FilePath mime_file(FILE_PATH_LITERAL("gdata/MimeFile.ext_2"));
  web_apps->GetWebAppsForFile(mime_file, "application/test_type_2",
                              &mime_ext_results);
  ASSERT_EQ(2U, mime_ext_results.size());
  VerifyApp1(mime_ext_results, true);
  VerifyApp2(mime_ext_results, true);
}

}  // namespace gdata
