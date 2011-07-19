// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_key_work_item.h"

#include <shlwapi.h>
#include <algorithm>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/win/registry.h"
#include "chrome/installer/util/logging_installer.h"

using base::win::RegKey;

namespace {
const REGSAM kKeyReadNoNotify = (KEY_READ) & ~(KEY_NOTIFY);
}

// A container for a registry key, its values, and its subkeys.  We don't use
// more obvious methods for various reasons:
// - RegCopyTree isn't supported pre-Vista, so we'd have to do something
//   different for XP anyway.
// - SHCopyKey can't copy subkeys into a volatile destination, so we'd have to
//   worry about polluting the registry.
// We don't persist security attributes since we only delete keys that we own,
// and we don't set custom attributes on them anyway.
class DeleteRegKeyWorkItem::RegKeyBackup {
 public:
  RegKeyBackup();
  bool Initialize(const RegKey& key);
  bool WriteTo(RegKey* key) const;

 private:
  // A container for a registry value.
  class RegValueBackup {
   public:
    RegValueBackup();
    void Initialize(const wchar_t* name_buffer, DWORD name_size,
                    DWORD type, const uint8* data, DWORD data_size);
    const std::wstring& name_str() const { return name_; }
    const wchar_t* name() const { return name_.empty() ? NULL : name_.c_str(); }
    DWORD type() const { return type_; }
    const uint8* data() const { return data_.empty() ? NULL : &data_[0]; }
    DWORD data_len() const { return static_cast<DWORD>(data_.size()); }

   private:
    std::wstring name_;
    std::vector<uint8> data_;
    DWORD type_;

    DISALLOW_COPY_AND_ASSIGN(RegValueBackup);
  };

  scoped_array<RegValueBackup> values_;
  scoped_array<std::wstring> subkey_names_;
  scoped_array<RegKeyBackup> subkeys_;
  ptrdiff_t num_values_;
  ptrdiff_t num_subkeys_;

  DISALLOW_COPY_AND_ASSIGN(RegKeyBackup);
};

DeleteRegKeyWorkItem::RegKeyBackup::RegValueBackup::RegValueBackup()
    : type_(REG_NONE) {
}

void DeleteRegKeyWorkItem::RegKeyBackup::RegValueBackup::Initialize(
    const wchar_t* name_buffer,
    DWORD name_size,
    DWORD type, const uint8* data,
    DWORD data_size) {
  name_.assign(name_buffer, name_size);
  type_ = type;
  data_.assign(data, data + data_size);
}

DeleteRegKeyWorkItem::RegKeyBackup::RegKeyBackup()
    : num_values_(0),
      num_subkeys_(0) {
}

// Initializes this object by reading the values and subkeys of |key|.
// Security descriptors are not backed up.
bool DeleteRegKeyWorkItem::RegKeyBackup::Initialize(const RegKey& key) {
  DCHECK(key.Valid());

  scoped_array<RegValueBackup> values;
  scoped_array<std::wstring> subkey_names;
  scoped_array<RegKeyBackup> subkeys;

  DWORD num_subkeys = 0;
  DWORD max_subkey_name_len = 0;
  DWORD num_values = 0;
  DWORD max_value_name_len = 0;
  DWORD max_value_len = 0;
  LONG result = RegQueryInfoKey(key.Handle(), NULL, NULL, NULL,
                                &num_subkeys, &max_subkey_name_len, NULL,
                                &num_values, &max_value_name_len,
                                &max_value_len, NULL, NULL);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed getting info of key to backup, result: " << result;
    return false;
  }
  if (max_subkey_name_len >= std::numeric_limits<DWORD>::max() - 1 ||
      max_value_name_len >= std::numeric_limits<DWORD>::max() - 1) {
    LOG(ERROR)
        << "Failed backing up key; subkeys and/or names are out of range.";
    return false;
  }
  DWORD max_name_len = std::max(max_subkey_name_len, max_value_name_len) + 1;
  scoped_array<wchar_t> name_buffer(new wchar_t[max_name_len]);

  // Backup the values.
  if (num_values != 0) {
    values.reset(new RegValueBackup[num_values]);
    scoped_array<uint8> value_buffer(new uint8[max_value_len]);
    DWORD name_size = 0;
    DWORD value_type = REG_NONE;
    DWORD value_size = 0;

    for (DWORD i = 0; i < num_values; ) {
      name_size = max_name_len;
      value_size = max_value_len;
      result = RegEnumValue(key.Handle(), i, name_buffer.get(), &name_size,
                            NULL, &value_type, value_buffer.get(), &value_size);
      switch (result) {
        case ERROR_NO_MORE_ITEMS:
          num_values = i;
          break;
        case ERROR_SUCCESS:
          values[i].Initialize(name_buffer.get(), name_size, value_type,
                               value_buffer.get(), value_size);
          ++i;
          break;
        case ERROR_MORE_DATA:
          if (value_size > max_value_len) {
            max_value_len = value_size;
            value_buffer.reset(new uint8[max_value_len]);
          } else {
            DCHECK(max_name_len - 1 < name_size);
            if (name_size >= std::numeric_limits<DWORD>::max() - 1) {
              LOG(ERROR) << "Failed backing up key; value name out of range.";
              return false;
            }
            max_name_len = name_size + 1;
            name_buffer.reset(new wchar_t[max_name_len]);
          }
          break;
        default:
          LOG(ERROR) << "Failed backing up value " << i << ", result: "
                     << result;
          return false;
      }
    }
    DLOG_IF(WARNING, RegEnumValue(key.Handle(), num_values, name_buffer.get(),
                                  &name_size, NULL, &value_type, NULL,
                                  NULL) != ERROR_NO_MORE_ITEMS)
        << "Concurrent modifications to registry key during backup operation.";
  }

  // Backup the subkeys.
  if (num_subkeys != 0) {
    subkey_names.reset(new std::wstring[num_subkeys]);
    subkeys.reset(new RegKeyBackup[num_subkeys]);
    DWORD name_size = 0;

    // Get the names of them.
    for (DWORD i = 0; i < num_subkeys; ) {
      name_size = max_name_len;
      result = RegEnumKeyEx(key.Handle(), i, name_buffer.get(), &name_size,
                            NULL, NULL, NULL, NULL);
      switch (result) {
        case ERROR_NO_MORE_ITEMS:
          num_subkeys = i;
          break;
        case ERROR_SUCCESS:
          subkey_names[i].assign(name_buffer.get(), name_size);
          ++i;
          break;
        case ERROR_MORE_DATA:
          if (name_size >= std::numeric_limits<DWORD>::max() - 1) {
            LOG(ERROR) << "Failed backing up key; subkey name out of range.";
            return false;
          }
          max_name_len = name_size + 1;
          name_buffer.reset(new wchar_t[max_name_len]);
          break;
        default:
          LOG(ERROR) << "Failed getting name of subkey " << i
                     << " for backup, result: " << result;
          return false;
      }
    }
    DLOG_IF(WARNING,
            RegEnumKeyEx(key.Handle(), num_subkeys, NULL, &name_size, NULL,
                         NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
        << "Concurrent modifications to registry key during backup operation.";

    // Get their values.
    RegKey subkey;
    for (DWORD i = 0; i < num_subkeys; ++i) {
      result = subkey.Open(key.Handle(), subkey_names[i].c_str(),
                           kKeyReadNoNotify);
      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Failed opening subkey \"" << subkey_names[i]
                   << "\" for backup, result: " << result;
        return false;
      }
      if (!subkeys[i].Initialize(subkey)) {
        LOG(ERROR) << "Failed backing up subkey \"" << subkey_names[i] << "\"";
        return false;
      }
    }
  }

  values_.swap(values);
  subkey_names_.swap(subkey_names);
  subkeys_.swap(subkeys);
  num_values_ = num_values;
  num_subkeys_ = num_subkeys;

  return true;
}

// Writes the values and subkeys of this object into |key|.
bool DeleteRegKeyWorkItem::RegKeyBackup::WriteTo(RegKey* key) const {
  LONG result = ERROR_SUCCESS;

  // Write the values.
  for (int i = 0; i < num_values_; ++i) {
    const RegValueBackup& value = values_[i];
    result = RegSetValueEx(key->Handle(), value.name(), 0, value.type(),
                           value.data(), value.data_len());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed writing value \"" << value.name_str()
                 << "\", result: " << result;
      return false;
    }
  }

  // Write the subkeys.
  RegKey subkey;
  for (int i = 0; i < num_subkeys_; ++i) {
    const std::wstring& name = subkey_names_[i];

    result = subkey.Create(key->Handle(), name.c_str(), KEY_WRITE);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed creating subkey \"" << name << "\", result: "
                 << result;
      return false;
    }
    if (!subkeys_[i].WriteTo(&subkey)) {
      LOG(ERROR) << "Failed writing subkey \"" << name << "\", result: "
                 << result;
      return false;
    }
  }

  return true;
}

DeleteRegKeyWorkItem::~DeleteRegKeyWorkItem() {
}

DeleteRegKeyWorkItem::DeleteRegKeyWorkItem(HKEY predefined_root,
                                           const std::wstring& path)
    : predefined_root_(predefined_root),
      path_(path) {
  // It's a safe bet that we don't want to delete one of the root trees.
  DCHECK(!path.empty());
}

bool DeleteRegKeyWorkItem::Do() {
  scoped_ptr<RegKeyBackup> backup;

  // Only try to make a backup if we're not configured to ignore failures.
  if (!ignore_failure_) {
    RegKey original_key;

    // Does the key exist?
    LONG result = original_key.Open(predefined_root_, path_.c_str(),
                                    kKeyReadNoNotify);
    if (result == ERROR_SUCCESS) {
      backup.reset(new RegKeyBackup());
      if (!backup->Initialize(original_key)) {
        LOG(ERROR) << "Failed to backup key at " << path_;
        return ignore_failure_;
      }
    } else if (result != ERROR_FILE_NOT_FOUND) {
      LOG(ERROR) << "Failed to open key at " << path_
                 << " to create backup, result: " << result;
      return ignore_failure_;
    }
  }

  // Delete the key.
  LONG result = SHDeleteKey(predefined_root_, path_.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete key at " << path_ << ", result: "
               << result;
    return ignore_failure_;
  }

  // We've succeeded, so remember any backup we may have made.
  backup_.swap(backup);

  return true;
}

void DeleteRegKeyWorkItem::Rollback() {
  if (ignore_failure_ || backup_.get() == NULL)
    return;

  // Delete anything in the key before restoring the backup in case someone else
  // put new data in the key after Do().
  LONG result = SHDeleteKey(predefined_root_, path_.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete key at " << path_ << " in rollback, "
                  "result: " << result;
  }

  // Restore the old contents.  The restoration takes on its default security
  // attributes; any custom attributes are lost.
  RegKey original_key;
  result = original_key.Create(predefined_root_, path_.c_str(), KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create original key at " << path_
               << " in rollback, result: " << result;
  } else {
    if (!backup_->WriteTo(&original_key))
      LOG(ERROR) << "Failed to restore key in rollback, result: " << result;
  }
}
