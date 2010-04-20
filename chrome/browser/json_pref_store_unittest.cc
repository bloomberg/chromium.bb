// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/test/data/resource.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/json_pref_store.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

class JsonPrefStoreTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.AppendASCII("JsonPrefStoreTest");

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("pref_service");
    ASSERT_TRUE(file_util::PathExists(data_dir_));
  }

  virtual void TearDown() {
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;
  // the path to the directory where the test data is stored
  FilePath data_dir_;
};

// Test fallback behavior on a nonexistent file.
TEST_F(JsonPrefStoreTest, NonExistentFile) {
  FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  JsonPrefStore pref_store(bogus_input_file);
  EXPECT_EQ(PrefStore::PREF_READ_ERROR_NO_FILE, pref_store.ReadPrefs());
  EXPECT_TRUE(pref_store.ReadOnly());
  EXPECT_TRUE(pref_store.Prefs()->empty());
  // Writing to a read-only store should return true, but do nothing.
  EXPECT_TRUE(pref_store.WritePrefs());
  EXPECT_FALSE(file_util::PathExists(bogus_input_file));
}

TEST_F(JsonPrefStoreTest, Basic) {
  ASSERT_TRUE(file_util::CopyFile(data_dir_.AppendASCII("read.json"),
                                  test_dir_.AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  FilePath input_file = test_dir_.AppendASCII("write.json");
  ASSERT_TRUE(file_util::PathExists(input_file));
  JsonPrefStore pref_store(input_file);
  ASSERT_EQ(PrefStore::PREF_READ_ERROR_NONE, pref_store.ReadPrefs());
  ASSERT_FALSE(pref_store.ReadOnly());
  DictionaryValue* prefs = pref_store.Prefs();

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  const wchar_t kNewWindowsInTabs[] = L"tabs.new_windows_in_tabs";
  const wchar_t kMaxTabs[] = L"tabs.max_tabs";
  const wchar_t kLongIntPref[] = L"long_int.pref";

  std::wstring cnn(L"http://www.cnn.com");

  std::wstring string_value;
  EXPECT_TRUE(prefs->GetString(prefs::kHomePage, &string_value));
  EXPECT_EQ(cnn, string_value);

  const wchar_t kSomeDirectory[] = L"some_directory";

  FilePath::StringType path;
  EXPECT_TRUE(prefs->GetString(kSomeDirectory, &path));
  EXPECT_EQ(FilePath::StringType(FILE_PATH_LITERAL("/usr/local/")), path);
  FilePath some_path(FILE_PATH_LITERAL("/usr/sbin/"));
  prefs->SetString(kSomeDirectory, some_path.value());
  EXPECT_TRUE(prefs->GetString(kSomeDirectory, &path));
  EXPECT_EQ(some_path.value(), path);

  // Test reading some other data types from sub-dictionaries.
  bool boolean;
  EXPECT_TRUE(prefs->GetBoolean(kNewWindowsInTabs, &boolean));
  EXPECT_TRUE(boolean);

  prefs->SetBoolean(kNewWindowsInTabs, false);
  EXPECT_TRUE(prefs->GetBoolean(kNewWindowsInTabs, &boolean));
  EXPECT_FALSE(boolean);

  int integer;
  EXPECT_TRUE(prefs->GetInteger(kMaxTabs, &integer));
  EXPECT_EQ(20, integer);
  prefs->SetInteger(kMaxTabs, 10);
  EXPECT_TRUE(prefs->GetInteger(kMaxTabs, &integer));
  EXPECT_EQ(10, integer);

  prefs->SetString(kLongIntPref, Int64ToWString(214748364842LL));
  EXPECT_TRUE(prefs->GetString(kLongIntPref, &string_value));
  EXPECT_EQ(214748364842LL, StringToInt64(WideToUTF16Hack(string_value)));

  // Serialize and compare to expected output.
  // SavePersistentPrefs uses ImportantFileWriter which needs a file thread.
  MessageLoop message_loop;
  ChromeThread file_thread(ChromeThread::FILE, &message_loop);
  FilePath output_file = input_file;
  FilePath golden_output_file = data_dir_.AppendASCII("write.golden.json");
  ASSERT_TRUE(file_util::PathExists(golden_output_file));
  ASSERT_TRUE(pref_store.WritePrefs());
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(file_util::TextContentsEqual(golden_output_file, output_file));
  ASSERT_TRUE(file_util::Delete(output_file, false));
}
