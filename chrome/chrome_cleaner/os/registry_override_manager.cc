// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/registry_override_manager.h"

#include <windows.h>

#include <shlwapi.h>
#include <stdint.h>

#include <string>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/test/test_branding.h"

namespace chrome_cleaner {

namespace {

// Describes a predefined registry key to override.
struct RegistryPredefinedKey {
  HKEY key;
  const base::char16* name;
};

// Predefined keys to override.
// HKEY_CURRENT_USER must be last because temporary keys are created under it.
const RegistryPredefinedKey kPredefinedKeys[] = {
    {HKEY_CLASSES_ROOT, L"HKEY_CLASSES_ROOT"},
    {HKEY_CURRENT_CONFIG, L"HKEY_CURRENT_CONFIG"},
    {HKEY_LOCAL_MACHINE, L"HKEY_LOCAL_MACHINE"},
    {HKEY_USERS, L"HKEY_USERS"},
    {HKEY_CURRENT_USER, L"HKEY_CURRENT_USER"},
};
const size_t kPredefinedKeysSize = base::size(kPredefinedKeys);

const base::char16 kTimestampDelimiter[] = L"$";

// Timeout to delete stale temporary keys, in hours.
const int kRegistryOverrideTimeout = 24;

// Initial size of the buffer that holds the name of a value, in characters.
const size_t kValueNameBufferSize = 256;

// Initial size of the buffer that holds the content of a value, in bytes.
const size_t kValueContentBufferSize = 512;

// Delete keys created for old temporary registries. |timestamp| should be
// base::Time::Now(), except for tests. |temp_registry_parent| is the key under
// which temporary registries are created.
void DeleteStaleTempKeys(const base::Time& timestamp,
                         const base::string16& temp_registry_parent) {
  base::win::RegKey test_root_key;
  LONG res = test_root_key.Open(HKEY_CURRENT_USER, temp_registry_parent.c_str(),
                                KEY_ALL_ACCESS);
  if (res != ERROR_SUCCESS) {
    // This will occur on first-run, but is harmless.
    DCHECK_EQ(res, ERROR_FILE_NOT_FOUND);
    return;
  }

  base::win::RegistryKeyIterator iterator_test_root_key(
      HKEY_CURRENT_USER, temp_registry_parent.c_str());
  for (; iterator_test_root_key.Valid(); ++iterator_test_root_key) {
    const base::char16* key_name = iterator_test_root_key.Name();
    std::vector<base::string16> tokens =
        base::SplitString(key_name, base::string16(kTimestampDelimiter),
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    int64_t key_name_as_number = 0;

    if (!base::StringToInt64(tokens[0], &key_name_as_number)) {
      test_root_key.DeleteKey(key_name);
      continue;
    }

    base::Time key_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromNanoseconds(key_name_as_number));
    base::TimeDelta age = timestamp - key_time;

    if (age > base::TimeDelta::FromHours(kRegistryOverrideTimeout))
      test_root_key.DeleteKey(key_name);
  }
}

bool CloneKeyNoRecursion(const base::win::RegKey& source_key,
                         HKEY source_key_root,
                         const base::string16& source_key_path,
                         base::win::RegKey* destination_key) {
  DCHECK(source_key.Valid());
  DCHECK(destination_key);

  // Enumerate values and copy them to the temporary registry.
  std::vector<uint8_t> value_content(kValueContentBufferSize);
  std::vector<base::char16> value_name(kValueNameBufferSize);
  for (DWORD index = 0;; ++index) {
    DWORD value_name_size = static_cast<DWORD>(value_name.size());
    DWORD value_content_size = static_cast<DWORD>(value_content.size());
    DWORD type = REG_NONE;

    LONG res_enum = ::RegEnumValue(source_key.Handle(), index, &value_name[0],
                                   &value_name_size, nullptr, &type,
                                   &value_content[0], &value_content_size);
    if (res_enum == ERROR_MORE_DATA) {
      if (value_name_size > static_cast<DWORD>(value_name.size()))
        value_name.resize(value_name_size);

      if (value_content_size > static_cast<DWORD>(value_content.size()))
        value_content.resize(value_content_size);

      res_enum = ::RegEnumValue(source_key.Handle(), index, &value_name[0],
                                &value_name_size, nullptr, &type,
                                &value_content[0], &value_content_size);
    }

    if (res_enum == ERROR_NO_MORE_ITEMS) {
      break;
    } else if (res_enum != ERROR_SUCCESS) {
      DLOG(ERROR) << "Received error code " << res_enum
                  << " while reading a value to clone from key "
                  << source_key_path << " under root " << source_key_root
                  << ".";
      return false;
    }

    if (destination_key->WriteValue(&value_name[0], &value_content[0],
                                    value_content_size,
                                    type) != ERROR_SUCCESS) {
      DLOG(ERROR) << "Unable to write cloned value " << &value_name[0]
                  << " from key " << source_key_path << " under root "
                  << source_key_root << ".";
      return false;
    }
  }

  return true;
}

bool CloneKey(const RegistryOverrideManager::RegistryKeyToClone& key,
              const base::string16& temp_registry_path) {
  // Open the source key.
  base::win::RegKey source_key;
  LONG open_key_res =
      source_key.Open(key.root, key.path.c_str(), KEY_QUERY_VALUE);

  if (open_key_res == ERROR_FILE_NOT_FOUND) {
    // If the key doesn't exist, no need to clone it.
    return true;
  } else if (open_key_res != ERROR_SUCCESS) {
    DLOG(WARNING) << "The source key " << key.path << " under root " << key.root
                  << " cannot be opened to be cloned.";
    return false;
  }

  // Find the name of the root key for the key to clone.
  const base::char16* predefined_key_name = nullptr;
  for (size_t i = 0; i < kPredefinedKeysSize; ++i) {
    if (kPredefinedKeys[i].key == key.root) {
      predefined_key_name = kPredefinedKeys[i].name;
      break;
    }
  }
  DCHECK(predefined_key_name);

  // Create the destination key in the temporary registry.
  base::string16 destination_path =
      temp_registry_path + L"\\" + predefined_key_name + L"\\" + key.path;
  base::win::RegKey destination_key;
  destination_key.Create(HKEY_CURRENT_USER, destination_path.c_str(),
                         KEY_ALL_ACCESS);

  // Clone the key to the temporary registry.
  if (key.recursive) {
    return ::SHCopyKey(source_key.Handle(), L"", destination_key.Handle(), 0) ==
           ERROR_SUCCESS;
  } else {
    return CloneKeyNoRecursion(source_key, key.root, key.path,
                               &destination_key);
  }
}

base::string16 GetRegistryPath(const base::string16& temp_registry_id,
                               const base::string16& temp_registry_parent) {
  return temp_registry_parent + L"\\" + temp_registry_id;
}

}  // namespace

// Key under which temporary registries are created. Applications commonly put
// their custom keys under Software\[Company]\[Application]. The name of the
// leaf key is arbitrary. Since this class is used for interacting with tests,
// "Chrome Cleanup Tool Test" is always used as the product name.
const base::char16 RegistryOverrideManager::kTempRegistryParent[] =
    L"Software\\" COMPANY_SHORTNAME_STRING L"\\"
    TEST_PRODUCT_FULLNAME_STRING L"\\TempTestKeys";

RegistryOverrideManager::RegistryOverrideManager()
    : owns_temp_registry_(false), temp_registry_parent_(kTempRegistryParent) {
  DeleteStaleTempKeys(base::Time::Now(), temp_registry_parent_);
}

RegistryOverrideManager::~RegistryOverrideManager() {
  // Undo the overrides.
  for (size_t i = 0; i < overrides_.size(); ++i)
    ::RegOverridePredefKey(overrides_[i], nullptr);

  if (owns_temp_registry_) {
    // Delete the temporary key if it exists.
    DCHECK(!temp_registry_id_.empty());
    base::win::RegKey parent_key;
    if (parent_key.Open(HKEY_CURRENT_USER, temp_registry_parent_.c_str(),
                        KEY_ALL_ACCESS) == ERROR_SUCCESS) {
      parent_key.DeleteKey(temp_registry_id_.c_str());
    }
  }
}

bool RegistryOverrideManager::CreateTemporaryRegistry(
    const RegistryKeyVector& keys_to_clone) {
  if (!temp_registry_id_.empty())
    return false;
  temp_registry_id_ = GenerateTempRegistryId(base::Time::Now());
  owns_temp_registry_ = true;

  // Clone keys to the temporary registry.
  base::string16 temp_registry_path =
      GetRegistryPath(temp_registry_id_, temp_registry_parent_);
  for (size_t i = 0; i < keys_to_clone.size(); ++i) {
    if (!CloneKey(keys_to_clone[i], temp_registry_path))
      return false;
  }

  return true;
}

bool RegistryOverrideManager::CreateAndUseTemporaryRegistry(
    const RegistryKeyVector& keys_to_clone) {
  if (!temp_registry_id_.empty())
    return false;

  // Create the temporary registry.
  if (!CreateTemporaryRegistry(keys_to_clone))
    return false;

  // Enable the registry override.
  return UseTemporaryRegistryInternal();
}

bool RegistryOverrideManager::UseTemporaryRegistry(
    const base::string16& temp_registry_parent,
    const base::string16& temp_registry_id) {
  if (!temp_registry_id_.empty())
    return false;
  owns_temp_registry_ = false;
  temp_registry_parent_ = temp_registry_parent;
  temp_registry_id_ = temp_registry_id;
  return UseTemporaryRegistryInternal();
}

RegistryOverrideManager::RegistryOverrideManager(
    const base::Time& timestamp,
    const base::string16& temp_registry_parent)
    : owns_temp_registry_(false), temp_registry_parent_(temp_registry_parent) {
  DeleteStaleTempKeys(timestamp, temp_registry_parent_);
}

bool RegistryOverrideManager::UseTemporaryRegistryInternal() {
  base::string16 temp_registry_path =
      GetRegistryPath(temp_registry_id_, temp_registry_parent_);
  bool error = false;

  for (size_t i = 0; i < kPredefinedKeysSize; ++i) {
    const RegistryPredefinedKey& key = kPredefinedKeys[i];

    base::win::RegKey reg_key;
    base::string16 key_path = temp_registry_path + L"\\" + key.name;

    // Create the key, or open it if it already exists.
    reg_key.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_ALL_ACCESS);

    if (!reg_key.Valid() ||
        ::RegOverridePredefKey(key.key, reg_key.Handle()) != ERROR_SUCCESS) {
      error = true;
      continue;
    }

    overrides_.push_back(key.key);
  }
  return !error;
}

void RegistryOverrideManager::OverrideRegistryDuringTests(
    base::CommandLine* command_line) {
  if (command_line->HasSwitch(kUseTempRegistryPathSwitch)) {
    base::string16 temp_registry_path =
        command_line->GetSwitchValueNative(kUseTempRegistryPathSwitch);
    base::string16 temp_registry_parent;
    base::string16 temp_registry_id;
    ParseTempRegistryPath(temp_registry_path, &temp_registry_parent,
                          &temp_registry_id);
    UseTemporaryRegistry(temp_registry_parent, temp_registry_id);
  }
}

base::string16 RegistryOverrideManager::GetTempRegistryPath() const {
  return temp_registry_parent_ + L'\\' + temp_registry_id_;
}

// static
void RegistryOverrideManager::ParseTempRegistryPath(
    const base::string16& temp_registry_path,
    base::string16* temp_registry_parent,
    base::string16* temp_registry_id) {
  // |temp_registry_path| has the following format:
  //     Path\To\Temp\Registry\Root\RegistryId
  auto root_id_delimiter_pos = temp_registry_path.rfind(L'\\');
  DCHECK_NE(root_id_delimiter_pos, base::string16::npos);
  *temp_registry_parent = temp_registry_path.substr(0, root_id_delimiter_pos);
  *temp_registry_id = temp_registry_path.substr(root_id_delimiter_pos + 1);
}

// static.
base::string16 RegistryOverrideManager::GenerateTempRegistryId(
    const base::Time& timestamp) {
  base::string16 id = base::Int64ToString16(
      timestamp.ToDeltaSinceWindowsEpoch().InNanoseconds());
  id += kTimestampDelimiter + base::ASCIIToUTF16(base::GenerateGUID());
  return id;
}

}  // namespace chrome_cleaner
