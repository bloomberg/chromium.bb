// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/registry_key_backup.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/win/registry.h"

using base::win::RegKey;

namespace {
const REGSAM kKeyReadNoNotify = (KEY_READ) & ~(KEY_NOTIFY);
}  // namespace

// A container for a registry key, its values, and its subkeys.
class RegistryKeyBackup::KeyData {
 public:
  KeyData();
  ~KeyData();
  bool Initialize(const RegKey& key);
  bool WriteTo(RegKey* key) const;

 private:
  class ValueData;

  scoped_array<ValueData> values_;
  scoped_array<std::wstring> subkey_names_;
  scoped_array<KeyData> subkeys_;
  DWORD num_values_;
  DWORD num_subkeys_;

  DISALLOW_COPY_AND_ASSIGN(KeyData);
};

// A container for a registry value.
class RegistryKeyBackup::KeyData::ValueData {
 public:
  ValueData();
  ~ValueData();
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

  DISALLOW_COPY_AND_ASSIGN(ValueData);
};

RegistryKeyBackup::KeyData::ValueData::ValueData()
    : type_(REG_NONE) {
}

RegistryKeyBackup::KeyData::ValueData::~ValueData()
{
}

void RegistryKeyBackup::KeyData::ValueData::Initialize(
    const wchar_t* name_buffer,
    DWORD name_size,
    DWORD type,
    const uint8* data,
    DWORD data_size) {
  name_.assign(name_buffer, name_size);
  type_ = type;
  data_.assign(data, data + data_size);
}

RegistryKeyBackup::KeyData::KeyData()
    : num_values_(0),
      num_subkeys_(0) {
}

RegistryKeyBackup::KeyData::~KeyData()
{
}

// Initializes this object by reading the values and subkeys of |key|.
// Security descriptors are not backed up.
bool RegistryKeyBackup::KeyData::Initialize(const RegKey& key) {
  scoped_array<ValueData> values;
  scoped_array<std::wstring> subkey_names;
  scoped_array<KeyData> subkeys;

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
    values.reset(new ValueData[num_values]);
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
            value_buffer.reset();  // Release to heap before new allocation.
            value_buffer.reset(new uint8[max_value_len]);
          } else {
            DCHECK_LT(max_name_len - 1, name_size);
            if (name_size >= std::numeric_limits<DWORD>::max() - 1) {
              LOG(ERROR) << "Failed backing up key; value name out of range.";
              return false;
            }
            max_name_len = name_size + 1;
            name_buffer.reset();  // Release to heap before new allocation.
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
    subkeys.reset(new KeyData[num_subkeys]);
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
bool RegistryKeyBackup::KeyData::WriteTo(RegKey* key) const {
  DCHECK(key);

  LONG result = ERROR_SUCCESS;

  // Write the values.
  for (DWORD i = 0; i < num_values_; ++i) {
    const ValueData& value = values_[i];
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
  for (DWORD i = 0; i < num_subkeys_; ++i) {
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

RegistryKeyBackup::RegistryKeyBackup() {
}

RegistryKeyBackup::~RegistryKeyBackup() {
}

bool RegistryKeyBackup::Initialize(HKEY root, const wchar_t* key_path) {
  DCHECK(key_path);

  RegKey key;
  scoped_ptr<KeyData> key_data;

  // Does the key exist?
  LONG result = key.Open(root, key_path, kKeyReadNoNotify);
  if (result == ERROR_SUCCESS) {
    key_data.reset(new KeyData());
    if (!key_data->Initialize(key)) {
      LOG(ERROR) << "Failed to backup key at " << key_path;
      return false;
    }
  } else if (result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to open key at " << key_path
               << " to create backup, result: " << result;
    return false;
  }

  key_data_.swap(key_data);
  return true;
}

bool RegistryKeyBackup::WriteTo(HKEY root, const wchar_t* key_path) const {
  DCHECK(key_path);

  bool success = false;

  if (key_data_.get() != NULL) {
    RegKey dest_key;
    LONG result = dest_key.Create(root, key_path, KEY_WRITE);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to create destination key at " << key_path
                 << " to write backup, result: " << result;
    } else {
      success = key_data_->WriteTo(&dest_key);
      LOG_IF(ERROR, !success) << "Failed to write key data.";
    }
  } else {
    success = true;
  }

  return success;
}
