// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_provider_win.h"

#include <userenv.h>

#include <algorithm>

#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
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
    const wchar_t* value_name, string16* result) {
  DWORD value_size = 0;
  DWORD key_type = 0;
  scoped_array<uint8> buffer;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, policy::kRegistrySubKey);
  if (hkcu_policy_key.ReadValue(value_name, 0, &value_size, &key_type)) {
    if (key_type != REG_SZ)
      return false;
    // According to the Microsoft documentation, the string
    // buffer may not be explicitly 0-terminated. Allocate a
    // slightly larger buffer and prefill to zeros to guarantee
    // the 0-termination.
    buffer.reset(new uint8[value_size + 2]);
    memset(buffer.get(), 0, value_size + 2);
    hkcu_policy_key.ReadValue(value_name, buffer.get(), &value_size);
  } else {
    RegKey hklm_policy_key(HKEY_CURRENT_USER, policy::kRegistrySubKey);
    if (hklm_policy_key.ReadValue(value_name, 0, &value_size, &key_type)) {
      if (key_type != REG_SZ)
        return false;
      // According to the Microsoft documentation, the string
      // buffer may not be explicitly 0-terminated. Allocate a
      // slightly larger buffer and prefill to zeros to guarantee
      // the 0-termination.
      buffer.reset(new uint8[value_size + 2]);
      memset(buffer.get(), 0, value_size + 2);
      hklm_policy_key.ReadValue(value_name, buffer.get(), &value_size);
    } else {
      return false;
    }
  }

  result->assign(reinterpret_cast<const wchar_t*>(buffer.get()));
  return true;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyBoolean(
    const wchar_t* value_name, bool* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, policy::kRegistrySubKey);
  if (hkcu_policy_key.ReadValueDW(value_name, &value)) {
    *result = value != 0;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, policy::kRegistrySubKey);
  if (hklm_policy_key.ReadValueDW(value_name, &value)) {
    *result = value != 0;
    return true;
  }
  return false;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyInteger(
    const wchar_t* value_name, uint32* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, policy::kRegistrySubKey);
  if (hkcu_policy_key.ReadValueDW(value_name, &value)) {
    *result = value;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, policy::kRegistrySubKey);
  if (hklm_policy_key.ReadValueDW(value_name, &value)) {
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
    std::wstring string_value;
    uint32 int_value;
    bool bool_value;
    switch (current->value_type) {
      case Value::TYPE_STRING:
        if (GetRegistryPolicyString(name.c_str(), &string_value)) {
          store->Apply(
              current->policy_type,
              Value::CreateStringValue(string_value));
        }
        break;
      case Value::TYPE_BOOLEAN:
        if (GetRegistryPolicyBoolean(name.c_str(), &bool_value)) {
          store->Apply(current->policy_type,
                       Value::CreateBooleanValue(bool_value));
        }
        break;
      case Value::TYPE_INTEGER:
        if (GetRegistryPolicyInteger(name.c_str(), &int_value)) {
          store->Apply(current->policy_type,
                       Value::CreateIntegerValue(int_value));
        }
        break;
      default:
        NOTREACHED();
        return false;
    }
  }

  return true;
}
