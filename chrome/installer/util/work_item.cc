// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/work_item.h"

#include <windows.h>

#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/delete_reg_key_work_item.h"
#include "chrome/installer/util/delete_reg_value_work_item.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "chrome/installer/util/self_reg_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item_list.h"

WorkItem::WorkItem() : ignore_failure_(false) {
}

WorkItem::~WorkItem() {
}

CallbackWorkItem* WorkItem::CreateCallbackWorkItem(
    base::Callback<bool(const CallbackWorkItem&)> callback) {
  return new CallbackWorkItem(callback);
}

CopyTreeWorkItem* WorkItem::CreateCopyTreeWorkItem(
    const base::FilePath& source_path,
    const base::FilePath& dest_path,
    const base::FilePath& temp_dir,
    CopyOverWriteOption overwrite_option,
    const base::FilePath& alternative_path) {
  return new CopyTreeWorkItem(source_path, dest_path, temp_dir,
                              overwrite_option, alternative_path);
}

CreateDirWorkItem* WorkItem::CreateCreateDirWorkItem(
    const base::FilePath& path) {
  return new CreateDirWorkItem(path);
}

CreateRegKeyWorkItem* WorkItem::CreateCreateRegKeyWorkItem(
    HKEY predefined_root,
    const std::wstring& path,
    REGSAM wow64_access) {
  return new CreateRegKeyWorkItem(predefined_root, path, wow64_access);
}

DeleteRegKeyWorkItem* WorkItem::CreateDeleteRegKeyWorkItem(
    HKEY predefined_root,
    const std::wstring& path,
    REGSAM wow64_access) {
  return new DeleteRegKeyWorkItem(predefined_root, path, wow64_access);
}

DeleteRegValueWorkItem* WorkItem::CreateDeleteRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name) {
  return new DeleteRegValueWorkItem(
      predefined_root, key_path, wow64_access, value_name);
}

DeleteTreeWorkItem* WorkItem::CreateDeleteTreeWorkItem(
    const base::FilePath& root_path,
    const base::FilePath& temp_path,
    const std::vector<base::FilePath>& key_paths) {
  return new DeleteTreeWorkItem(root_path, temp_path, key_paths);
}

MoveTreeWorkItem* WorkItem::CreateMoveTreeWorkItem(
    const base::FilePath& source_path,
    const base::FilePath& dest_path,
    const base::FilePath& temp_dir,
    MoveTreeOption duplicate_option) {
  return new MoveTreeWorkItem(source_path,
                              dest_path,
                              temp_dir,
                              duplicate_option);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    const std::wstring& value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root,
                                 key_path,
                                 wow64_access,
                                 value_name,
                                 value_data,
                                 overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    DWORD value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root,
                                 key_path,
                                 wow64_access,
                                 value_name,
                                 value_data,
                                 overwrite);
}

SetRegValueWorkItem* WorkItem::CreateSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    int64 value_data,
    bool overwrite) {
  return new SetRegValueWorkItem(predefined_root,
                                 key_path,
                                 wow64_access,
                                 value_name,
                                 value_data,
                                 overwrite);
}

SelfRegWorkItem* WorkItem::CreateSelfRegWorkItem(const std::wstring& dll_path,
                                                 bool do_register,
                                                 bool user_level_registration) {
  return new SelfRegWorkItem(dll_path, do_register, user_level_registration);
}

WorkItemList* WorkItem::CreateWorkItemList() {
  return new WorkItemList();
}

// static
WorkItemList* WorkItem::CreateNoRollbackWorkItemList() {
  return new NoRollbackWorkItemList();
}

WorkItemList* WorkItem::CreateConditionalWorkItemList(Condition* condition) {
  return new ConditionalWorkItemList(condition);
}
