// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>  // NOLINT

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/registry.h"
#include "chrome/installer/util/copy_reg_key_work_item.h"
#include "chrome/installer/util/registry_test_data.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

class CopyRegKeyWorkItemTest : public testing::Test {
 protected:
  static void TearDownTestCase() {
    logging::CloseLogFile();
  }

  virtual void SetUp() {
    ASSERT_TRUE(test_data_.Initialize(HKEY_CURRENT_USER, L"SOFTWARE\\TmpTmp"));
    destination_path_.assign(test_data_.base_path()).append(L"\\Destination");
  }

  RegistryTestData test_data_;
  std::wstring destination_path_;
};

// Test that copying a key that doesn't exist succeeds, and that rollback does
// nothing.
TEST_F(CopyRegKeyWorkItemTest, TestNoKey) {
  const std::wstring key_paths[] = {
    std::wstring(test_data_.base_path() + L"\\NoKeyHere"),
    std::wstring(test_data_.base_path() + L"\\NoKeyHere\\OrHere")
  };
  RegKey key;
  for (size_t i = 0; i < arraysize(key_paths); ++i) {
    const std::wstring& key_path = key_paths[i];
    scoped_ptr<CopyRegKeyWorkItem> item(
        WorkItem::CreateCopyRegKeyWorkItem(test_data_.root_key(), key_path,
                                           destination_path_));
    EXPECT_TRUE(item->Do());
    EXPECT_NE(ERROR_SUCCESS,
              key.Open(test_data_.root_key(), destination_path_.c_str(),
                       KEY_READ));
    item->Rollback();
    item.reset();
    EXPECT_NE(ERROR_SUCCESS,
              key.Open(test_data_.root_key(), destination_path_.c_str(),
                       KEY_READ));
  }
}

// Test that copying an empty key succeeds, and that rollback removes the copy.
TEST_F(CopyRegKeyWorkItemTest, TestEmptyKey) {
  RegKey key;
  scoped_ptr<CopyRegKeyWorkItem> item(
      WorkItem::CreateCopyRegKeyWorkItem(test_data_.root_key(),
                                         test_data_.empty_key_path(),
                                         destination_path_));
  EXPECT_TRUE(item->Do());
  RegistryTestData::ExpectEmptyKey(test_data_.root_key(),
                                   destination_path_.c_str());
  item->Rollback();
  item.reset();
  EXPECT_NE(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), destination_path_.c_str(),
                     KEY_READ));
}

// Test that copying a key with subkeys and values succeeds, and that rollback
// removes the copy.
TEST_F(CopyRegKeyWorkItemTest, TestNonEmptyKey) {
  RegKey key;
  scoped_ptr<CopyRegKeyWorkItem> item(
      WorkItem::CreateCopyRegKeyWorkItem(test_data_.root_key(),
                                         test_data_.non_empty_key_path(),
                                         destination_path_));
  EXPECT_TRUE(item->Do());
  test_data_.ExpectMatchesNonEmptyKey(test_data_.root_key(),
                                      destination_path_.c_str());
  item->Rollback();
  item.reset();
  EXPECT_NE(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), destination_path_.c_str(),
                     KEY_READ));
}

// Test that copying an empty key over a key with data succeeds, and that
// rollback restores the original data.
TEST_F(CopyRegKeyWorkItemTest, TestOverwriteAndRestore) {
  RegKey key;
  // First copy the data into the dest.
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(test_data_.root_key(), destination_path_.c_str(),
                       KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS,
            SHCopyKey(test_data_.root_key(),
                      test_data_.non_empty_key_path().c_str(), key.Handle(),
                      0));
  key.Close();

  // Now copy the empty key into the dest.
  scoped_ptr<CopyRegKeyWorkItem> item(
      WorkItem::CreateCopyRegKeyWorkItem(test_data_.root_key(),
                                         test_data_.empty_key_path(),
                                         destination_path_));
  EXPECT_TRUE(item->Do());

  // Make sure the dest is now empty.
  RegistryTestData::ExpectEmptyKey(test_data_.root_key(),
                                   destination_path_.c_str());

  // Restore the data.
  item->Rollback();
  item.reset();

  // Make sure it's all there.
  test_data_.ExpectMatchesNonEmptyKey(test_data_.root_key(),
                                      destination_path_.c_str());
}

// Test that Rollback does nothing when the item is configured to ignore
// failures.
TEST_F(CopyRegKeyWorkItemTest, TestIgnoreFailRollback) {
  // Copy the empty key onto the non-empty key, ignoring failures.
  scoped_ptr<CopyRegKeyWorkItem> item(
      WorkItem::CreateCopyRegKeyWorkItem(test_data_.root_key(),
                                         test_data_.empty_key_path(),
                                         test_data_.non_empty_key_path()));
  item->set_ignore_failure(true);
  EXPECT_TRUE(item->Do());
  item->Rollback();
  item.reset();
  RegistryTestData::ExpectEmptyKey(test_data_.root_key(),
                                   test_data_.non_empty_key_path().c_str());
}
