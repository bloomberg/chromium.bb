// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H__
#define CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H__

#include <windows.h>

#include <string>
#include <vector>

#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that sets a registry value with REG_SZ, REG_DWORD, or
// REG_QWORD type at the specified path. The value is only set if the target key
// exists.
class SetRegValueWorkItem : public WorkItem {
 public:
  virtual ~SetRegValueWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  enum SettingStatus {
    // The status before Do is called.
    SET_VALUE,
    // One possible outcome after Do(). A new value is created under the key.
    NEW_VALUE_CREATED,
    // One possible outcome after Do(). The previous value under the key has
    // been overwritten.
    VALUE_OVERWRITTEN,
    // One possible outcome after Do(). No change is applied, either
    // because we are not allowed to overwrite the previous value, or due to
    // some errors like the key does not exist.
    VALUE_UNCHANGED,
    // The status after Do and Rollback is called.
    VALUE_ROLL_BACK
  };

  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      const std::wstring& value_data,
                      bool overwrite);

  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      DWORD value_data,
                      bool overwrite);

  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      int64 value_data,
                      bool overwrite);

  // Root key of the target key under which the value is set. The root key can
  // only be one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the target key under which the value is set.
  std::wstring key_path_;

  // Name of the value to be set.
  std::wstring value_name_;

  // Whether to overwrite the existing value under the target key.
  bool overwrite_;

  // Whether to force 32-bit or 64-bit view of the target key.
  REGSAM wow64_access_;

  // Type of data to store
  DWORD type_;
  std::vector<uint8> value_;
  DWORD previous_type_;
  std::vector<uint8> previous_value_;

  SettingStatus status_;
};

#endif  // CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H__
