// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_loader_win.h"

#include <string>

#include <string.h>

#include <userenv.h>

// userenv.dll is required for RegisterGPNotification().
#pragma comment(lib, "userenv.lib")

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"

using base::win::RegKey;

namespace policy {

namespace {

// Determines the registry key with the highest priority that contains
// the |key_path| key with a |value_name| value inside.
// |key_path| is a suffix to the Chrome mandatory or recommended registry key,
// and can be empty to lookup values directly at that key.
// |value_name| is the name of a value that should exist at that path. If it
// is empty, then only the existence of the path is verified.
// Returns true if |key| was updated to point to a key with a |key_path| suffix
// and optional |value_name| value inside. In that case, |level| and |scope|
// will contain the appropriate values for the key found.
// Returns false otherwise.
bool LoadHighestPriorityKey(const string16& key_path,
                            const string16& value_name,
                            RegKey* key,
                            PolicyLevel* level,
                            PolicyScope* scope) {
  // |path| is in decreasing order of priority.
  static const struct {
    const wchar_t* path;
    PolicyLevel level;
  } key_paths[] = {
    { kRegistryMandatorySubKey,   POLICY_LEVEL_MANDATORY    },
    { kRegistryRecommendedSubKey, POLICY_LEVEL_RECOMMENDED  },
  };

  // |hive| is in decreasing order of priority.
  static const struct {
    HKEY hive;
    PolicyScope scope;
  } hives[] = {
    { HKEY_LOCAL_MACHINE,   POLICY_SCOPE_MACHINE  },
    { HKEY_CURRENT_USER,    POLICY_SCOPE_USER     },
  };

  // Lookup at the mandatory path for both user and machine policies first, and
  // then at the recommended path.
  for (size_t k = 0; k < arraysize(key_paths); ++k) {
    for (size_t h = 0; h < arraysize(hives); ++h) {
      string16 path(key_paths[k].path);
      if (!key_path.empty())
        path += ASCIIToUTF16("\\") + key_path;
      key->Open(hives[h].hive, path.c_str(), KEY_READ);
      if (!key->Valid())
        continue;
      if (value_name.empty() || key->HasValue(value_name.c_str())) {
        *level = key_paths[k].level;
        *scope = hives[h].scope;
        return true;
      }
    }
  }
  return false;
}

// Reads a REG_SZ string at |key| named |name| into |result|. Returns false if
// the string could not be read.
bool ReadRegistryString(RegKey* key,
                        const string16& name,
                        string16* result) {
  DWORD value_size = 0;
  DWORD key_type = 0;
  scoped_array<uint8> buffer;

  if (key->ReadValue(name.c_str(), 0, &value_size, &key_type) != ERROR_SUCCESS)
    return false;
  if (key_type != REG_SZ)
    return false;

  // According to the Microsoft documentation, the string
  // buffer may not be explicitly 0-terminated. Allocate a
  // slightly larger buffer and pre-fill to zeros to guarantee
  // the 0-termination.
  buffer.reset(new uint8[value_size + 2]);
  memset(buffer.get(), 0, value_size + 2);
  key->ReadValue(name.c_str(), buffer.get(), &value_size, NULL);
  result->assign(reinterpret_cast<const wchar_t*>(buffer.get()));
  return true;
}

// Reads a REG_DWORD integer at |key| named |name| into |result|. Returns false
// if the value could no be read.
bool ReadRegistryInteger(RegKey* key,
                         const string16& name,
                         uint32* result) {
  DWORD dword;
  if (key->ReadValueDW(name.c_str(), &dword) != ERROR_SUCCESS)
    return false;
  *result = dword;
  return true;
}

// Returns the Value for a Chrome string policy named |name|, or NULL if
// it wasn't found. The caller owns the returned value.
base::Value* ReadStringValue(const string16& name,
                             PolicyLevel* level,
                             PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  string16 value;
  if (!ReadRegistryString(&key, name, &value))
    return NULL;
  return base::Value::CreateStringValue(value);
}

// Returns the Value for a Chrome string list policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadStringListValue(const string16& name,
                                 PolicyLevel* level,
                                 PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(name, string16(), &key, level, scope))
    return NULL;
  base::ListValue* result = new base::ListValue();
  string16 value;
  int index = 0;
  while (ReadRegistryString(&key, base::IntToString16(++index), &value))
    result->Append(base::Value::CreateStringValue(value));
  return result;
}

// Returns the Value for a Chrome boolean policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadBooleanValue(const string16& name,
                              PolicyLevel* level,
                              PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  uint32 value;
  if (!ReadRegistryInteger(&key, name, &value))
    return NULL;
  return base::Value::CreateBooleanValue(value != 0u);
}

// Returns the Value for a Chrome integer policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadIntegerValue(const string16& name,
                              PolicyLevel* level,
                              PolicyScope* scope) {
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  uint32 value;
  if (!ReadRegistryInteger(&key, name, &value))
    return NULL;
  return base::Value::CreateIntegerValue(value);
}

// Returns the Value for a Chrome dictionary policy named |name|,
// or NULL if it wasn't found. The caller owns the returned value.
base::Value* ReadDictionaryValue(const string16& name,
                                 PolicyLevel* level,
                                 PolicyScope* scope) {
  // Dictionaries are encoded as JSON strings on Windows.
  //
  // A dictionary could be stored as a subkey, with each of its entries
  // within that subkey. However, it would be impossible to recover the
  // type for some of those entries:
  //  - Booleans are stored as DWORDS and are indistinguishable from
  //    integers;
  //  - Lists are stored as a subkey, with entries mapping 0 to N-1 to
  //    their value. This is indistinguishable from a Dictionary with
  //    integer keys.
  //
  // The GPO policy editor also has a limited data entry form that doesn't
  // support dictionaries.
  RegKey key;
  if (!LoadHighestPriorityKey(string16(), name, &key, level, scope))
    return NULL;
  string16 value;
  if (!ReadRegistryString(&key, name, &value))
    return NULL;
  return base::JSONReader::Read(UTF16ToUTF8(value));
}

}  // namespace

PolicyLoaderWin::PolicyLoaderWin(const PolicyDefinitionList* policy_list)
    : is_initialized_(false),
      policy_list_(policy_list),
      user_policy_changed_event_(false, false),
      machine_policy_changed_event_(false, false),
      user_policy_watcher_failed_(false),
      machine_policy_watcher_failed_(false) {
  if (!RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
    DPLOG(WARNING) << "Failed to register user group policy notification";
    user_policy_watcher_failed_ = true;
  }
  if (!RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
    DPLOG(WARNING) << "Failed to register machine group policy notification.";
    machine_policy_watcher_failed_ = true;
  }
}

PolicyLoaderWin::~PolicyLoaderWin() {
  user_policy_watcher_.StopWatching();
  machine_policy_watcher_.StopWatching();
}

void PolicyLoaderWin::InitOnFile() {
  is_initialized_ = true;
  SetupWatches();
}

scoped_ptr<PolicyBundle> PolicyLoaderWin::Load() {
  // Reset the watches BEFORE reading the individual policies to avoid
  // missing a change notification.
  if (is_initialized_)
    SetupWatches();

  scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
  PolicyMap& chrome_policy = bundle->Get(POLICY_DOMAIN_CHROME, std::string());

  const PolicyDefinitionList::Entry* current;
  for (current = policy_list_->begin; current != policy_list_->end; ++current) {
    const string16 name(ASCIIToUTF16(current->name));
    PolicyLevel level = POLICY_LEVEL_MANDATORY;
    PolicyScope scope = POLICY_SCOPE_MACHINE;
    Value* value = NULL;

    switch (current->value_type) {
      case Value::TYPE_STRING:
        value = ReadStringValue(name, &level, &scope);
        break;

      case Value::TYPE_LIST:
        value = ReadStringListValue(name, &level, &scope);
        break;

      case Value::TYPE_BOOLEAN:
        value = ReadBooleanValue(name, &level, &scope);
        break;

      case Value::TYPE_INTEGER:
        value = ReadIntegerValue(name, &level, &scope);
        break;

      case Value::TYPE_DICTIONARY:
        value = ReadDictionaryValue(name, &level, &scope);
        break;

      default:
        NOTREACHED();
    }

    if (value)
      chrome_policy.Set(current->name, level, scope, value);
  }

  return bundle.Pass();
}

void PolicyLoaderWin::SetupWatches() {
  DCHECK(is_initialized_);
  if (!user_policy_watcher_failed_ &&
      !user_policy_watcher_.GetWatchedObject() &&
      !user_policy_watcher_.StartWatching(
          user_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for user policy change event";
    user_policy_watcher_failed_ = true;
  }
  if (!machine_policy_watcher_failed_ &&
      !machine_policy_watcher_.GetWatchedObject() &&
      !machine_policy_watcher_.StartWatching(
          machine_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for machine policy change event";
    machine_policy_watcher_failed_ = true;
  }
}

void PolicyLoaderWin::OnObjectSignaled(HANDLE object) {
  DCHECK(object == user_policy_changed_event_.handle() ||
         object == machine_policy_changed_event_.handle())
      << "unexpected object signaled policy reload, obj = "
      << std::showbase << std::hex << object;
  Reload(false);
}

}  // namespace policy
