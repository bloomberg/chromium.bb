// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Base class for managing an action of a sequence of actions to be carried
// out during install/update/uninstall. Supports rollback of actions if this
// process fails.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_WORK_ITEM_H_

#include <windows.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"

class CallbackWorkItem;
class CopyTreeWorkItem;
class CreateDirWorkItem;
class CreateRegKeyWorkItem;
class DeleteTreeWorkItem;
class DeleteRegKeyWorkItem;
class DeleteRegValueWorkItem;
class MoveTreeWorkItem;
class SelfRegWorkItem;
class SetRegValueWorkItem;
class WorkItemList;

namespace base {
class FilePath;
}

// A base class that defines APIs to perform/rollback an action or a
// sequence of actions during install/update/uninstall.
class WorkItem {
 public:
  // All registry operations can be instructed to operate on a specific view
  // of the registry by specifying a REGSAM value to the wow64_access parameter.
  // The wow64_access parameter can be one of:
  // KEY_WOW64_32KEY - Operate on the 32-bit view.
  // KEY_WOW64_64KEY - Operate on the 64-bit view.
  // kWow64Default   - Operate on the default view (e.g. 32-bit on 32-bit
  //                   systems, and 64-bit on 64-bit systems).
  // See http://msdn.microsoft.com/en-us/library/windows/desktop/aa384129.aspx
  static const REGSAM kWow64Default = 0;
  // Possible states
  enum CopyOverWriteOption {
    ALWAYS,  // Always overwrite regardless of what existed before.
    NEVER,  // Not used currently.
    IF_DIFFERENT,  // Overwrite if different. Currently only applies to file.
    IF_NOT_PRESENT,  // Copy only if file/directory do not exist already.
    NEW_NAME_IF_IN_USE  // Copy to a new path if dest is in use(only files).
  };

  // Options for the MoveTree work item.
  enum MoveTreeOption {
    ALWAYS_MOVE,  // Always attempt to do a move operation.
    CHECK_DUPLICATES  // Only move if the move target is different.
  };

  // Abstract base class for the conditions used by ConditionWorkItemList.
  // TODO(robertshield): Move this out of WorkItem.
  class Condition {
   public:
    virtual ~Condition() {}
    virtual bool ShouldRun() const = 0;
  };

  virtual ~WorkItem();

  // Create a CallbackWorkItem that invokes a callback.
  static CallbackWorkItem* CreateCallbackWorkItem(
      base::Callback<bool(const CallbackWorkItem&)> callback);

  // Create a CopyTreeWorkItem that recursively copies a file system hierarchy
  // from source path to destination path.
  // * If overwrite_option is ALWAYS, the created CopyTreeWorkItem always
  //   overwrites files.
  // * If overwrite_option is NEW_NAME_IF_IN_USE, file is copied with an
  //   alternate name specified by alternative_path.
  static CopyTreeWorkItem* CreateCopyTreeWorkItem(
      const base::FilePath& source_path,
      const base::FilePath& dest_path,
      const base::FilePath& temp_dir,
      CopyOverWriteOption overwrite_option,
      const base::FilePath& alternative_path);

  // Create a CreateDirWorkItem that creates a directory at the given path.
  static CreateDirWorkItem* CreateCreateDirWorkItem(const base::FilePath& path);

  // Create a CreateRegKeyWorkItem that creates a registry key at the given
  // path.
  static CreateRegKeyWorkItem* CreateCreateRegKeyWorkItem(
      HKEY predefined_root,
      const std::wstring& path,
      REGSAM wow64_access);

  // Create a DeleteRegKeyWorkItem that deletes a registry key at the given
  // path.
  static DeleteRegKeyWorkItem* CreateDeleteRegKeyWorkItem(
      HKEY predefined_root,
      const std::wstring& path,
      REGSAM wow64_access);

  // Create a DeleteRegValueWorkItem that deletes a registry value
  static DeleteRegValueWorkItem* CreateDeleteRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name);

  // Create a DeleteTreeWorkItem that recursively deletes a file system
  // hierarchy at the given root path. A key file can be optionally specified
  // by key_path.
  static DeleteTreeWorkItem* CreateDeleteTreeWorkItem(
      const base::FilePath& root_path,
      const base::FilePath& temp_path,
      const std::vector<base::FilePath>& key_paths);

  // Create a MoveTreeWorkItem that recursively moves a file system hierarchy
  // from source path to destination path.
  static MoveTreeWorkItem* CreateMoveTreeWorkItem(
      const base::FilePath& source_path,
      const base::FilePath& dest_path,
      const base::FilePath& temp_dir,
      MoveTreeOption duplicate_option);

  // Create a SetRegValueWorkItem that sets a registry value with REG_SZ type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      const std::wstring& value_data,
      bool overwrite);

  // Create a SetRegValueWorkItem that sets a registry value with REG_DWORD type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      DWORD value_data,
      bool overwrite);

  // Create a SetRegValueWorkItem that sets a registry value with REG_QWORD type
  // at the key with specified path.
  static SetRegValueWorkItem* CreateSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      int64 value_data,
      bool overwrite);

  // Add a SelfRegWorkItem that registers or unregisters a DLL at the
  // specified path.
  static SelfRegWorkItem* CreateSelfRegWorkItem(const std::wstring& dll_path,
                                                bool do_register,
                                                bool user_level_registration);

  // Create an empty WorkItemList. A WorkItemList can recursively contains
  // a list of WorkItems.
  static WorkItemList* CreateWorkItemList();

  // Create an empty WorkItemList that cannot be rolled back.
  // Such a work item list executes all items on a best effort basis and does
  // not abort execution if an item in the list fails.
  static WorkItemList* CreateNoRollbackWorkItemList();

  // Create a conditional work item list that will execute only if
  // condition->ShouldRun() returns true. The WorkItemList instance
  // assumes ownership of condition.
  static WorkItemList* CreateConditionalWorkItemList(Condition* condition);

  // Perform the actions of WorkItem. Returns true if success, returns false
  // otherwise.
  // If the WorkItem is transactional, then Do() is done as a transaction.
  // If it returns false, there will be no change on the system.
  virtual bool Do() = 0;

  // Rollback any actions previously carried out by this WorkItem. If the
  // WorkItem is transactional, then the previous actions can be fully
  // rolled back. If the WorkItem is non-transactional, the rollback is a
  // best effort.
  virtual void Rollback() = 0;

  // If called with true, this WorkItem may return true from its Do() method
  // even on failure and Rollback will have no effect.
  void set_ignore_failure(bool ignore_failure) {
    ignore_failure_ = ignore_failure;
  }

  // Returns true if this WorkItem should ignore failures.
  bool ignore_failure() const {
    return ignore_failure_;
  }

  // Sets an optional log message that a work item may use to print additional
  // instance-specific information.
  void set_log_message(const std::string& log_message) {
    log_message_ = log_message;
  }

  // Retrieves the optional log message. The retrieved string may be empty.
  const std::string& log_message() const { return log_message_; }

 protected:
  WorkItem();

  // Specifies whether this work item my fail to complete and yet still
  // return true from Do().
  bool ignore_failure_;

  std::string log_message_;
};

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_H_
