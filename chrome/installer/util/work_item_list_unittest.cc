// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

wchar_t test_root[] = L"ListList";
wchar_t data_str[] = L"data_111";

class WorkItemListTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary key for testing
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(test_root);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, test_root, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
        key.Create(HKEY_CURRENT_USER, test_root, KEY_READ));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    logging::CloseLogFile();

    // Clean up the temporary key
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(test_root));
  }

  ScopedTempDir temp_dir_;
};

}  // namespace

// Execute a WorkItem list successfully and then rollback.
TEST_F(WorkItemListTest, ExecutionSuccess) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(file_util::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"ExecutionSuccess");

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring name(L"name");
  std::wstring data(data_str);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, key_to_create,
                                          name, data, false)));
  work_item_list->AddWorkItem(work_item.release());

  EXPECT_TRUE(work_item_list->Do());

  // Verify all WorkItems have been executed.
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(data_str));
  key.Close();
  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
}

// Execute a WorkItem list. Fail in the middle. Rollback what has been done.
TEST_F(WorkItemListTest, ExecutionFailAndRollback) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(file_util::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"ExecutionFail");

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring not_created_key(test_root);
  file_util::AppendToPath(&not_created_key, L"NotCreated");
  std::wstring name(L"name");
  std::wstring data(data_str);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, not_created_key,
                                          name, data, false)));
  work_item_list->AddWorkItem(work_item.release());

  // This one will not be executed because we will fail early.
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER,
                                           not_created_key)));
  work_item_list->AddWorkItem(work_item.release());

  EXPECT_FALSE(work_item_list->Do());

  // Verify the first 2 WorkItems have been executed.
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  key.Close();
  EXPECT_TRUE(file_util::PathExists(dir_to_create));
  // The last one should not be there.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, not_created_key.c_str(), KEY_READ));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
}

TEST_F(WorkItemListTest, ConditionalExecutionSuccess) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(file_util::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  scoped_ptr<WorkItemList> conditional_work_item_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(dir_to_create)));

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"ExecutionSuccess");
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  std::wstring name(L"name");
  std::wstring data(data_str);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, key_to_create,
                                          name, data, false)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  work_item_list->AddWorkItem(conditional_work_item_list.release());

  EXPECT_TRUE(work_item_list->Do());

  // Verify all WorkItems have been executed.
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(data_str));
  key.Close();
  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
}

TEST_F(WorkItemListTest, ConditionalExecutionConditionFailure) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(file_util::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  scoped_ptr<WorkItemList> conditional_work_item_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(dir_to_create.AppendASCII("c"))));

  std::wstring key_to_create(test_root);
  file_util::AppendToPath(&key_to_create, L"ExecutionSuccess");
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateRegKeyWorkItem(HKEY_CURRENT_USER, key_to_create)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  std::wstring name(L"name");
  std::wstring data(data_str);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, key_to_create,
                                          name, data, false)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  work_item_list->AddWorkItem(conditional_work_item_list.release());

  EXPECT_TRUE(work_item_list->Do());

  // Verify that the WorkItems added as part of the conditional list have NOT
  // been executed.
  RegKey key;
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  std::wstring read_out;
  EXPECT_NE(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  key.Close();

  // Verify that the other work item was executed.
  EXPECT_TRUE(file_util::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(file_util::PathExists(top_dir_to_create));
}


