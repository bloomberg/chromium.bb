// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

const wchar_t kTestRoot[] = L"ListList";
const wchar_t kDataStr[] = L"data_111";
const wchar_t kName[] = L"name";

class WorkItemListTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Create a temporary key for testing
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(kTestRoot);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kTestRoot, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
        key.Create(HKEY_CURRENT_USER, kTestRoot, KEY_READ));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    logging::CloseLogFile();

    // Clean up the temporary key
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kTestRoot));
  }

  base::ScopedTempDir temp_dir_;
};

}  // namespace

// Execute a WorkItem list successfully and then rollback.
TEST_F(WorkItemListTest, ExecutionSuccess) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  base::FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  base::FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(base::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring key_to_create(kTestRoot);
  key_to_create.push_back(base::FilePath::kSeparators[0]);
  key_to_create.append(L"ExecutionSuccess");

  work_item.reset(
      reinterpret_cast<WorkItem*>(WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create, WorkItem::kWow64Default)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring name(kName);
  std::wstring data(kDataStr);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
                                          key_to_create,
                                          WorkItem::kWow64Default,
                                          name,
                                          data,
                                          false)));
  work_item_list->AddWorkItem(work_item.release());

  EXPECT_TRUE(work_item_list->Do());

  // Verify all WorkItems have been executed.
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(kDataStr));
  key.Close();
  EXPECT_TRUE(base::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(base::PathExists(top_dir_to_create));
}

// Execute a WorkItem list. Fail in the middle. Rollback what has been done.
TEST_F(WorkItemListTest, ExecutionFailAndRollback) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  base::FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  base::FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(base::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring key_to_create(kTestRoot);
  key_to_create.push_back(base::FilePath::kSeparators[0]);
  key_to_create.append(L"ExecutionFail");

  work_item.reset(
      reinterpret_cast<WorkItem*>(WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create, WorkItem::kWow64Default)));
  work_item_list->AddWorkItem(work_item.release());

  std::wstring not_created_key(kTestRoot);
  not_created_key.push_back(base::FilePath::kSeparators[0]);
  not_created_key.append(L"NotCreated");
  std::wstring name(kName);
  std::wstring data(kDataStr);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
                                          not_created_key,
                                          WorkItem::kWow64Default,
                                          name,
                                          data,
                                          false)));
  work_item_list->AddWorkItem(work_item.release());

  // This one will not be executed because we will fail early.
  work_item.reset(
      reinterpret_cast<WorkItem*>(WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, not_created_key, WorkItem::kWow64Default)));
  work_item_list->AddWorkItem(work_item.release());

  EXPECT_FALSE(work_item_list->Do());

  // Verify the first 2 WorkItems have been executed.
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  key.Close();
  EXPECT_TRUE(base::PathExists(dir_to_create));
  // The last one should not be there.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, not_created_key.c_str(), KEY_READ));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(base::PathExists(top_dir_to_create));
}

TEST_F(WorkItemListTest, ConditionalExecutionSuccess) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  base::FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  base::FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(base::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  scoped_ptr<WorkItemList> conditional_work_item_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(dir_to_create)));

  std::wstring key_to_create(kTestRoot);
  key_to_create.push_back(base::FilePath::kSeparators[0]);
  key_to_create.append(L"ExecutionSuccess");
  work_item.reset(
      reinterpret_cast<WorkItem*>(WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create, WorkItem::kWow64Default)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  std::wstring name(kName);
  std::wstring data(kDataStr);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
                                          key_to_create,
                                          WorkItem::kWow64Default,
                                          name,
                                          data,
                                          false)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  work_item_list->AddWorkItem(conditional_work_item_list.release());

  EXPECT_TRUE(work_item_list->Do());

  // Verify all WorkItems have been executed.
  RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(name.c_str(), &read_out));
  EXPECT_EQ(0, read_out.compare(kDataStr));
  key.Close();
  EXPECT_TRUE(base::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(base::PathExists(top_dir_to_create));
}

TEST_F(WorkItemListTest, ConditionalExecutionConditionFailure) {
  scoped_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  scoped_ptr<WorkItem> work_item;

  base::FilePath top_dir_to_create(temp_dir_.path());
  top_dir_to_create = top_dir_to_create.AppendASCII("a");
  base::FilePath dir_to_create(top_dir_to_create);
  dir_to_create = dir_to_create.AppendASCII("b");
  ASSERT_FALSE(base::PathExists(dir_to_create));

  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateCreateDirWorkItem(dir_to_create)));
  work_item_list->AddWorkItem(work_item.release());

  scoped_ptr<WorkItemList> conditional_work_item_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(dir_to_create.AppendASCII("c"))));

  std::wstring key_to_create(kTestRoot);
  key_to_create.push_back(base::FilePath::kSeparators[0]);
  key_to_create.append(L"ExecutionSuccess");
  work_item.reset(
      reinterpret_cast<WorkItem*>(WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create, WorkItem::kWow64Default)));
  conditional_work_item_list->AddWorkItem(work_item.release());

  std::wstring name(kName);
  std::wstring data(kDataStr);
  work_item.reset(reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER,
                                          key_to_create,
                                          WorkItem::kWow64Default,
                                          name,
                                          data,
                                          false)));
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
  EXPECT_TRUE(base::PathExists(dir_to_create));

  work_item_list->Rollback();

  // Verify everything is rolled back.
  // The value must have been deleted first in roll back otherwise the key
  // can not be deleted.
  EXPECT_NE(ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, key_to_create.c_str(), KEY_READ));
  EXPECT_FALSE(base::PathExists(top_dir_to_create));
}
