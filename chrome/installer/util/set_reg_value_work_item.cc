// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/set_reg_value_work_item.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/logging_installer.h"

SetRegValueWorkItem::~SetRegValueWorkItem() {
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         const std::wstring& value_name,
                                         const std::wstring& value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      overwrite_(overwrite),
      status_(SET_VALUE),
      type_(REG_SZ),
      previous_type_(0) {
  const uint8* data = reinterpret_cast<const uint8*>(value_data.c_str());
  value_.assign(data, data + (value_data.length() + 1) * sizeof(wchar_t));
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         const std::wstring& value_name,
                                         DWORD value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      overwrite_(overwrite),
      status_(SET_VALUE),
      type_(REG_DWORD),
      previous_type_(0) {
  const uint8* data = reinterpret_cast<const uint8*>(&value_data);
  value_.assign(data, data + sizeof(value_data));
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         const std::wstring& value_name,
                                         int64 value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      overwrite_(overwrite),
      status_(SET_VALUE),
      type_(REG_QWORD),
      previous_type_(0) {
  const uint8* data = reinterpret_cast<const uint8*>(&value_data);
  value_.assign(data, data + sizeof(value_data));
}

bool SetRegValueWorkItem::Do() {
  LONG result = ERROR_SUCCESS;
  base::win::RegKey key;
  if (status_ != SET_VALUE) {
    // we already did something.
    VLOG(1) << "multiple calls to Do()";
    result = ERROR_CANTWRITE;
    return ignore_failure_;
  }

  status_ = VALUE_UNCHANGED;
  result = key.Open(predefined_root_, key_path_.c_str(),
                    KEY_READ | KEY_SET_VALUE);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "can not open " << key_path_ << " error: " << result;
    return ignore_failure_;
  }

  DWORD type = 0;
  DWORD size = 0;
  result = key.ReadValue(value_name_.c_str(), NULL, &size, &type);
  // If the value exists but we don't want to overwrite then there's
  // nothing more to do.
  if ((result != ERROR_FILE_NOT_FOUND) && !overwrite_) {
    return true;
  }

  // If there's something to be saved, save it.
  if (result == ERROR_SUCCESS) {
    if (!size) {
      previous_type_ = type;
    } else {
      previous_value_.resize(size);
      result = key.ReadValue(value_name_.c_str(), &previous_value_[0], &size,
                             &previous_type_);
      if (result != ERROR_SUCCESS) {
        previous_value_.clear();
        VLOG(1) << "Failed to save original value. Error: " << result;
      }
    }
  }

  result = key.WriteValue(value_name_.c_str(), &value_[0],
                          static_cast<DWORD>(value_.size()), type_);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "Failed to write value " << key_path_ << " error: " << result;
    return ignore_failure_;
  }

  status_ = previous_type_ ? VALUE_OVERWRITTEN : NEW_VALUE_CREATED;
  return true;
}

void SetRegValueWorkItem::Rollback() {
  if (ignore_failure_)
    return;

  if (status_ == SET_VALUE || status_ == VALUE_ROLL_BACK)
    return;

  if (status_ == VALUE_UNCHANGED) {
    status_ = VALUE_ROLL_BACK;
    VLOG(1) << "rollback: setting unchanged, nothing to do";
    return;
  }

  base::win::RegKey key;
  LONG result = key.Open(predefined_root_, key_path_.c_str(), KEY_SET_VALUE);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "rollback: can not open " << key_path_ << " error: " << result;
    return;
  }

  if (status_ == NEW_VALUE_CREATED) {
    result = key.DeleteValue(value_name_.c_str());
    VLOG(1) << "rollback: deleting " << value_name_ << " error: " << result;
  } else if (status_ == VALUE_OVERWRITTEN) {
    const unsigned char* previous_value =
        previous_value_.empty() ? NULL : &previous_value_[0];
    result = key.WriteValue(value_name_.c_str(), previous_value,
                            static_cast<DWORD>(previous_value_.size()),
                            previous_type_);
    VLOG(1) << "rollback: restoring " << value_name_ << " error: " << result;
  } else {
    NOTREACHED();
  }

  status_ = VALUE_ROLL_BACK;
}
