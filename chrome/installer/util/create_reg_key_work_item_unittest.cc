// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

wchar_t test_root[] = L"TmpTmp";

class CreateRegKeyWorkItemTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary key for testing
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(test_root);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, test_root,
                                        KEY_READ));
  }
  virtual void TearDown() {
    logging::CloseLogFile();
    // Clean up the temporary key
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(test_root));
  }
};

}  // namespace

TEST_F(CreateRegKeyWorkItemTest, CreateKey) {
  RegKey key;

  FilePath parent_key(test_root);
  parent_key = parent_key.AppendASCII("a");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.value().c_str(), KEY_READ));

  FilePath top_key_to_create(parent_key);
  top_key_to_create = top_key_to_create.AppendASCII("b");

  FilePath key_to_create(top_key_to_create);
  key_to_create = key_to_create.AppendASCII("c");
  key_to_create = key_to_create.AppendASCII("d");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create.value()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.value().c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should delete all the keys up to top_key_to_create.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, top_key_to_create.value().c_str(), KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, parent_key.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, CreateExistingKey) {
  RegKey key;

  FilePath key_to_create(test_root);
  key_to_create = key_to_create.AppendASCII("aa");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, key_to_create.value().c_str(), KEY_READ));

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create.value()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.value().c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should not remove the key since it exists before
  // the CreateRegKeyWorkItem is called.
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, CreateSharedKey) {
  RegKey key;
  FilePath key_to_create_1(test_root);
  key_to_create_1 = key_to_create_1.AppendASCII("aaa");

  FilePath key_to_create_2(key_to_create_1);
  key_to_create_2 = key_to_create_2.AppendASCII("bbb");

  FilePath key_to_create_3(key_to_create_2);
  key_to_create_3 = key_to_create_3.AppendASCII("ccc");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create_3.value()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_3.value().c_str(), KEY_READ));

  // Create another key under key_to_create_2
  FilePath key_to_create_4(key_to_create_2);
  key_to_create_4 = key_to_create_4.AppendASCII("ddd");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, key_to_create_4.value().c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should delete key_to_create_3.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_3.value().c_str(), KEY_READ));

  // Rollback should not delete key_to_create_2 as it is shared.
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_2.value().c_str(), KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_4.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, RollbackWithMissingKey) {
  RegKey key;
  FilePath key_to_create_1(test_root);
  key_to_create_1 = key_to_create_1.AppendASCII("aaaa");

  FilePath key_to_create_2(key_to_create_1);
  key_to_create_2 = key_to_create_2.AppendASCII("bbbb");

  FilePath key_to_create_3(key_to_create_2);
  key_to_create_3 = key_to_create_3.AppendASCII("cccc");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create_3.value()));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_3.value().c_str(), KEY_READ));
  key.Close();

  // now delete key_to_create_3
  ASSERT_EQ(ERROR_SUCCESS,
      RegDeleteKey(HKEY_CURRENT_USER, key_to_create_3.value().c_str()));
  ASSERT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_3.value().c_str(), KEY_READ));

  work_item->Rollback();

  // key_to_create_3 has already been deleted, Rollback should delete
  // the rest.
  ASSERT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create_1.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, RollbackWithSetValue) {
  RegKey key;

  FilePath key_to_create(test_root);
  key_to_create = key_to_create.AppendASCII("aaaaa");

  scoped_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           key_to_create.value()));

  EXPECT_TRUE(work_item->Do());

  // Write a value under the key we just created.
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
      key_to_create.value().c_str(), KEY_READ | KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"name", L"value"));
  key.Close();

  work_item->Rollback();

  // Rollback should not remove the key.
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.value().c_str(), KEY_READ));
}
