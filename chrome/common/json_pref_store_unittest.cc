// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/memory/scoped_temp_dir.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

class JsonPrefStoreTest : public testing::Test {
 protected:
  virtual void SetUp() {
    message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("pref_service");
    ASSERT_TRUE(file_util::PathExists(data_dir_));
  }

  // The path to temporary directory used to contain the test operations.
  ScopedTempDir temp_dir_;
  // The path to the directory where the test data is stored.
  FilePath data_dir_;
  // A message loop that we can use as the file thread message loop.
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

// Test fallback behavior for a nonexistent file.
TEST_F(JsonPrefStoreTest, NonExistentFile) {
  FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  ASSERT_FALSE(file_util::PathExists(bogus_input_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(bogus_input_file, message_loop_proxy_.get());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
}

// Test fallback behavior for an invalid file.
TEST_F(JsonPrefStoreTest, InvalidFile) {
  FilePath invalid_file_original = data_dir_.AppendASCII("invalid.json");
  FilePath invalid_file = temp_dir_.path().AppendASCII("invalid.json");
  ASSERT_TRUE(file_util::CopyFile(invalid_file_original, invalid_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(invalid_file, message_loop_proxy_.get());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());

  // The file should have been moved aside.
  EXPECT_FALSE(file_util::PathExists(invalid_file));
  FilePath moved_aside = temp_dir_.path().AppendASCII("invalid.bad");
  EXPECT_TRUE(file_util::PathExists(moved_aside));
  EXPECT_TRUE(file_util::TextContentsEqual(invalid_file_original,
                                           moved_aside));
}

TEST_F(JsonPrefStoreTest, Basic) {
  ASSERT_TRUE(file_util::CopyFile(data_dir_.AppendASCII("read.json"),
                                  temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(file_util::PathExists(input_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(input_file, message_loop_proxy_.get());
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  ASSERT_FALSE(pref_store->ReadOnly());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  const char kNewWindowsInTabs[] = "tabs.new_windows_in_tabs";
  const char kMaxTabs[] = "tabs.max_tabs";
  const char kLongIntPref[] = "long_int.pref";

  std::string cnn("http://www.cnn.com");

  const Value* actual;
  EXPECT_EQ(PrefStore::READ_OK,
            pref_store->GetValue(prefs::kHomePage, &actual));
  std::string string_value;
  EXPECT_TRUE(actual->GetAsString(&string_value));
  EXPECT_EQ(cnn, string_value);

  const char kSomeDirectory[] = "some_directory";

  EXPECT_EQ(PrefStore::READ_OK, pref_store->GetValue(kSomeDirectory, &actual));
  FilePath::StringType path;
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(FilePath::StringType(FILE_PATH_LITERAL("/usr/local/")), path);
  FilePath some_path(FILE_PATH_LITERAL("/usr/sbin/"));

  pref_store->SetValue(kSomeDirectory,
                       Value::CreateStringValue(some_path.value()));
  EXPECT_EQ(PrefStore::READ_OK, pref_store->GetValue(kSomeDirectory, &actual));
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(some_path.value(), path);

  // Test reading some other data types from sub-dictionaries.
  EXPECT_EQ(PrefStore::READ_OK,
            pref_store->GetValue(kNewWindowsInTabs, &actual));
  bool boolean = false;
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_TRUE(boolean);

  pref_store->SetValue(kNewWindowsInTabs,
                      Value::CreateBooleanValue(false));
  EXPECT_EQ(PrefStore::READ_OK,
            pref_store->GetValue(kNewWindowsInTabs, &actual));
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_FALSE(boolean);

  EXPECT_EQ(PrefStore::READ_OK, pref_store->GetValue(kMaxTabs, &actual));
  int integer = 0;
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(20, integer);
  pref_store->SetValue(kMaxTabs, Value::CreateIntegerValue(10));
  EXPECT_EQ(PrefStore::READ_OK, pref_store->GetValue(kMaxTabs, &actual));
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(10, integer);

  pref_store->SetValue(kLongIntPref,
                      Value::CreateStringValue(
                          base::Int64ToString(214748364842LL)));
  EXPECT_EQ(PrefStore::READ_OK, pref_store->GetValue(kLongIntPref, &actual));
  EXPECT_TRUE(actual->GetAsString(&string_value));
  int64 value;
  base::StringToInt64(string_value, &value);
  EXPECT_EQ(214748364842LL, value);

  // Serialize and compare to expected output.
  FilePath output_file = input_file;
  FilePath golden_output_file = data_dir_.AppendASCII("write.golden.json");
  ASSERT_TRUE(file_util::PathExists(golden_output_file));
  ASSERT_TRUE(pref_store->WritePrefs());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(file_util::TextContentsEqual(golden_output_file, output_file));
  ASSERT_TRUE(file_util::Delete(output_file, false));
}
