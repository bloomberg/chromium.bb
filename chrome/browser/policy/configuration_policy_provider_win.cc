// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_win.h"

#include <userenv.h>

#include <algorithm>

#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/policy_constants.h"

ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    GroupPolicyChangeWatcher(ConfigurationPolicyProvider* provider)
        : provider_(provider),
          user_policy_changed_event_(false, false),
          machine_policy_changed_event_(false, false) {
  CHECK(RegisterGPNotification(user_policy_changed_event_.handle(), false));
  CHECK(RegisterGPNotification(machine_policy_changed_event_.handle(), true));
  user_policy_watcher_.StartWatching(
      user_policy_changed_event_.handle(),
      this);
  machine_policy_watcher_.StartWatching(
      machine_policy_changed_event_.handle(),
      this);
}

void ConfigurationPolicyProviderWin::GroupPolicyChangeWatcher::
    OnObjectSignaled(HANDLE object) {
  provider_->NotifyStoreOfPolicyChange();
  if (object == user_policy_changed_event_.handle()) {
    user_policy_watcher_.StartWatching(
        user_policy_changed_event_.handle(),
        this);
  } else if (object == machine_policy_changed_event_.handle()) {
    machine_policy_watcher_.StartWatching(
        machine_policy_changed_event_.handle(),
        this);
  } else {
    // This method should only be called as a result of signals
    // to the user- and machine-policy handle watchers.
    NOTREACHED();
  }
}

ConfigurationPolicyProviderWin::ConfigurationPolicyProviderWin() {
  watcher_.reset(new GroupPolicyChangeWatcher(this));
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyString(
    const string16& name, int index, string16* result) {
  DWORD value_size = 0;
  DWORD key_type = 0;
  scoped_array<uint8> buffer;
  RegKey policy_key;
  string16 location = string16(policy::kRegistrySubKey);
  string16 value_name = name;

  if (index > 0) {
    // This is a list value, treat |name| as a subkey.
    location += ASCIIToUTF16("\\") + name;
    value_name = base::IntToString16(index);
  }

  // First try the global policy.
  if (!policy_key.Open(HKEY_LOCAL_MACHINE, location.c_str()) ||
      !policy_key.ReadValue(value_name.c_str(), 0, &value_size, &key_type)) {
    policy_key.Close();
    // Fall back on user-specific policy.
    if (!policy_key.Open(HKEY_CURRENT_USER, location.c_str()) ||
        !policy_key.ReadValue(value_name.c_str(), 0, &value_size, &key_type)) {
      return false;
    }
  }

  if (key_type != REG_SZ)
    return false;

  // According to the Microsoft documentation, the string
  // buffer may not be explicitly 0-terminated. Allocate a
  // slightly larger buffer and pre-fill to zeros to guarantee
  // the 0-termination.
  buffer.reset(new uint8[value_size + 2]);
  memset(buffer.get(), 0, value_size + 2);
  policy_key.ReadValue(value_name.c_str(), buffer.get(), &value_size);
  result->assign(reinterpret_cast<const wchar_t*>(buffer.get()));
  return true;
}


bool ConfigurationPolicyProviderWin::GetRegistryPolicyStringList(
    const string16& key, ListValue* result) {
  int index = 0;
  string16 policy_string;
  while (GetRegistryPolicyString(key, ++index,  &policy_string))
    result->Append(Value::CreateStringValue(policy_string));
  return true;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyBoolean(
    const string16& value_name, bool* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, policy::kRegistrySubKey);
  if (hkcu_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value != 0;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, policy::kRegistrySubKey);
  if (hklm_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value != 0;
    return true;
  }
  return false;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyInteger(
    const string16& value_name, uint32* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, policy::kRegistrySubKey);
  if (hkcu_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, policy::kRegistrySubKey);
  if (hklm_policy_key.ReadValueDW(value_name.c_str(), &value)) {
    *result = value;
    return true;
  }
  return false;
}

bool ConfigurationPolicyProviderWin::Provide(
    ConfigurationPolicyStore* store) {
  const PolicyValueMap* mapping = PolicyValueMapping();

  for (PolicyValueMap::const_iterator current = mapping->begin();
       current != mapping->end(); ++current) {
    std::wstring name = UTF8ToWide(current->name);
    switch (current->value_type) {
      case Value::TYPE_STRING: {
        std::wstring string_value;
        if (GetRegistryPolicyString(name.c_str(), -1, &string_value)) {
          store->Apply(current->policy_type,
                       Value::CreateStringValue(string_value));
        }
        break;
      }
      case Value::TYPE_LIST: {
        scoped_ptr<ListValue> list_value(new ListValue);
        if (GetRegistryPolicyStringList(name.c_str(), list_value.get()))
          store->Apply(current->policy_type, list_value.release());
        break;
      }
      case Value::TYPE_BOOLEAN: {
        bool bool_value;
        if (GetRegistryPolicyBoolean(name.c_str(), &bool_value)) {
          store->Apply(current->policy_type,
                       Value::CreateBooleanValue(bool_value));
        }
        break;
      }
      case Value::TYPE_INTEGER: {
        uint32 int_value;
        if (GetRegistryPolicyInteger(name.c_str(), &int_value)) {
          store->Apply(current->policy_type,
                       Value::CreateIntegerValue(int_value));
        }
        break;
      }
      default:
        NOTREACHED();
        return false;
    }
  }

  return true;
}
