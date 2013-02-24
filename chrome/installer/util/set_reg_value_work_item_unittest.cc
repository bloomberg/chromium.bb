// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

const wchar_t kTestRoot[] = L"TempTemp";
const wchar_t kDataStr1[] = L"data_111";
const wchar_t kDataStr2[] = L"data_222";
const wchar_t kNameStr[] = L"name_str";
const wchar_t kNameDword[] = L"name_dword";

const DWORD dword1 = 0;
const DWORD dword2 = 1;

class SetRegValueWorkItemTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary key for testing
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(kTestRoot);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kTestRoot, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
        key.Create(HKEY_CURRENT_USER, kTestRoot, KEY_READ));
  }
  virtual void TearDown() {
    logging::CloseLogFile();
    // Clean up the temporary key
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kTestRoot));
  }
};

}  // namespace

// Write a new value without overwrite flag. The value should be set.
TEST_F(SetRegValueWorkItemTest, WriteNewNonOverwrite) {
  RegKey key;

  std::wstring parent_key(kTestRoot);
  parent_key.append(&base::FilePath::kSeparators[0], 1);
  parent_key.append(L"WriteNewNonOverwrite");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ));

  std::wstring name_str(kNameStr);
  std::wstring data_str(kDataStr1);
  scoped_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_str, data_str, false));

  std::wstring name_dword(kNameDword);
  scoped_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_dword, dword1, false));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  std::wstring read_out;
  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name_str.c_str(), &read_out));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name_dword.c_str(), &read_dword));
  EXPECT_EQ(read_out, kDataStr1);
  EXPECT_EQ(read_dword, dword1);

  work_item1->Rollback();
  work_item2->Rollback();

  // Rollback should delete the value.
  EXPECT_FALSE(key.HasValue(name_str.c_str()));
  EXPECT_FALSE(key.HasValue(name_dword.c_str()));
}

// Write a new value with overwrite flag. The value should be set.
TEST_F(SetRegValueWorkItemTest, WriteNewOverwrite) {
  RegKey key;

  std::wstring parent_key(kTestRoot);
  parent_key.append(&base::FilePath::kSeparators[0], 1);
  parent_key.append(L"WriteNewOverwrite");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.c_str(), KEY_READ));

  std::wstring name_str(kNameStr);
  std::wstring data_str(kDataStr1);
  scoped_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_str, data_str, true));

  std::wstring name_dword(kNameDword);
  scoped_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_dword, dword1, true));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  std::wstring read_out;
  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name_str.c_str(), &read_out));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name_dword.c_str(), &read_dword));
  EXPECT_EQ(read_out, kDataStr1);
  EXPECT_EQ(read_dword, dword1);

  work_item1->Rollback();
  work_item2->Rollback();

  // Rollback should delete the value.
  EXPECT_FALSE(key.HasValue(name_str.c_str()));
  EXPECT_FALSE(key.HasValue(name_dword.c_str()));
}

// Write to an existing value without overwrite flag. There should be
// no change.
TEST_F(SetRegValueWorkItemTest, WriteExistingNonOverwrite) {
  RegKey key;

  std::wstring parent_key(kTestRoot);
  parent_key.append(&base::FilePath::kSeparators[0], 1);
  parent_key.append(L"WriteExistingNonOverwrite");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.c_str(),
                 KEY_READ | KEY_SET_VALUE));

  // First test REG_SZ value.
  // Write data to the value we are going to set.
  std::wstring name(kNameStr);
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name.c_str(), kDataStr1));

  std::wstring data(kDataStr2);
  scoped_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name, data, false));
  EXPECT_TRUE(work_item->Do());

  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(kDataStr1));

  work_item->Rollback();
  EXPECT_TRUE(key.HasValue(name.c_str()));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  // Now test REG_DWORD value.
  // Write data to the value we are going to set.
  name.assign(kNameDword);
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name.c_str(), dword1));
  work_item.reset(WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
      parent_key, name, dword2, false));
  EXPECT_TRUE(work_item->Do());

  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword1);

  work_item->Rollback();
  EXPECT_TRUE(key.HasValue(name.c_str()));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword1);
}

// Write to an existing value with overwrite flag. The value should be
// overwritten.
TEST_F(SetRegValueWorkItemTest, WriteExistingOverwrite) {
  RegKey key;

  std::wstring parent_key(kTestRoot);
  parent_key.append(&base::FilePath::kSeparators[0], 1);
  parent_key.append(L"WriteExistingOverwrite");
  ASSERT_EQ(ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, parent_key.c_str(),
                 KEY_READ | KEY_SET_VALUE));

  // First test REG_SZ value.
  // Write data to the value we are going to set.
  std::wstring name(kNameStr);
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name.c_str(), kDataStr1));

  std::wstring name_empty(L"name_empty");
  ASSERT_EQ(ERROR_SUCCESS, RegSetValueEx(key.Handle(), name_empty.c_str(), NULL,
                                         REG_SZ, NULL, 0));

  std::wstring data(kDataStr2);
  scoped_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name, data, true));
  scoped_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name_empty, data, true));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(kDataStr2));

  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name_empty.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(kDataStr2));

  work_item1->Rollback();
  work_item2->Rollback();

  EXPECT_TRUE(key.HasValue(name.c_str()));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  DWORD type = 0;
  DWORD size = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name_empty.c_str(), NULL, &size,
                                         &type));
  EXPECT_EQ(REG_SZ, type);
  EXPECT_EQ(0, size);

  // Now test REG_DWORD value.
  // Write data to the value we are going to set.
  name.assign(kNameDword);
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(name.c_str(), dword1));
  scoped_ptr<SetRegValueWorkItem> work_item3(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key, name,
                                          dword2, true));
  EXPECT_TRUE(work_item3->Do());

  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword2);

  work_item3->Rollback();
  EXPECT_TRUE(key.HasValue(name.c_str()));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(name.c_str(), &read_dword));
  EXPECT_EQ(read_dword, dword1);
}

// Write a value to a non-existing key. This should fail.
TEST_F(SetRegValueWorkItemTest, WriteNonExistingKey) {
  RegKey key;

  std::wstring parent_key(kTestRoot);
  parent_key.append(&base::FilePath::kSeparators[0], 1);
  parent_key.append(L"WriteNonExistingKey");

  std::wstring name(L"name");
  std::wstring data(kDataStr1);
  scoped_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, parent_key,
                                          name, data, false));
  EXPECT_FALSE(work_item->Do());

  work_item.reset(WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
      parent_key, name, dword1, false));
  EXPECT_FALSE(work_item->Do());
}
