// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/delete_reg_value_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

wchar_t test_root[] = L"DeleteRegValueWorkItemTest";

class DeleteRegValueWorkItemTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary key for testing
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(test_root);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
        key.Create(HKEY_CURRENT_USER, test_root, KEY_READ));
  }
  virtual void TearDown() {
    logging::CloseLogFile();
    // Clean up the temporary key
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(test_root));
  }
};

}  // namespace

// Delete a value. The value should get deleted after Do() and should be
// recreated after Rollback().
TEST_F(DeleteRegValueWorkItemTest, DeleteExistingValue) {
  RegKey key;
  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteNew");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ | KEY_WRITE));
  std::wstring name_str(L"name_str");
  std::wstring data_str(L"data_111");
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name_str.c_str(), data_str.c_str()));
  std::wstring name_dword(L"name_dword");
  DWORD data_dword = 100;
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name_dword.c_str(), data_dword));

  std::wstring name_empty(L"name_empty");
  ASSERT_EQ(ERROR_SUCCESS, RegSetValueEx(key.Handle(), name_empty.c_str(), NULL,
                                         REG_SZ, NULL, 0));

  scoped_ptr<DeleteRegValueWorkItem> work_item1(
      WorkItem::CreateDeleteRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                             name_str));
  scoped_ptr<DeleteRegValueWorkItem> work_item2(
      WorkItem::CreateDeleteRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                             name_dword));
  scoped_ptr<DeleteRegValueWorkItem> work_item3(
      WorkItem::CreateDeleteRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                             name_empty));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());
  EXPECT_TRUE(work_item3->Do());

  EXPECT_FALSE(key.ValueExists(name_str.c_str()));
  EXPECT_FALSE(key.ValueExists(name_dword.c_str()));
  EXPECT_FALSE(key.ValueExists(name_empty.c_str()));

  work_item1->Rollback();
  work_item2->Rollback();
  work_item3->Rollback();

  std::wstring read_str;
  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name_str.c_str(), &read_str));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name_dword.c_str(), &read_dword));
  EXPECT_EQ(read_str, data_str);
  EXPECT_EQ(read_dword, data_dword);

  // Verify empty value.
  DWORD type = 0;
  DWORD size = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name_empty.c_str(), NULL, &size,
                                         &type));
  EXPECT_EQ(REG_SZ, type);
  EXPECT_EQ(0, size);
}

// Try deleting a value that doesn't exist.
TEST_F(DeleteRegValueWorkItemTest, DeleteNonExistentValue) {
  RegKey key;
  std::wstring parent_key(test_root);
  file_util::AppendToPath(&parent_key, L"WriteNew");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ | KEY_WRITE));
  std::wstring name_str(L"name_str");
  std::wstring name_dword(L"name_dword");
  EXPECT_FALSE(key.ValueExists(name_str.c_str()));
  EXPECT_FALSE(key.ValueExists(name_dword.c_str()));

  scoped_ptr<DeleteRegValueWorkItem> work_item1(
      WorkItem::CreateDeleteRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                             name_str));
  scoped_ptr<DeleteRegValueWorkItem> work_item2(
      WorkItem::CreateDeleteRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                             name_dword));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  EXPECT_FALSE(key.ValueExists(name_str.c_str()));
  EXPECT_FALSE(key.ValueExists(name_dword.c_str()));

  work_item1->Rollback();
  work_item2->Rollback();

  EXPECT_FALSE(key.ValueExists(name_str.c_str()));
  EXPECT_FALSE(key.ValueExists(name_dword.c_str()));
}
