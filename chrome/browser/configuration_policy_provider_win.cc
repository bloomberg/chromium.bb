// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/configuration_policy_provider_win.h"

#include <algorithm>

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t ConfigurationPolicyProviderWin::kPolicyRegistrySubKey[] =
    L"SOFTWARE\\Policies\\Google\\Chrome";
#else
const wchar_t ConfigurationPolicyProviderWin::kPolicyRegistrySubKey[] =
    L"SOFTWARE\\Policies\\Chromium";
#endif

ConfigurationPolicyProviderWin::ConfigurationPolicyProviderWin() {
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyString(
    const wchar_t* value_name, string16* result) {
  DWORD value_size = 0;
  DWORD key_type = 0;
  scoped_array<uint8> buffer;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, kPolicyRegistrySubKey);
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
    RegKey hklm_policy_key(HKEY_CURRENT_USER, kPolicyRegistrySubKey);
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
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, kPolicyRegistrySubKey);
  if (hkcu_policy_key.ReadValueDW(value_name, &value)) {
    *result = value != 0;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, kPolicyRegistrySubKey);
  if (hklm_policy_key.ReadValueDW(value_name, &value)) {
    *result = value != 0;
    return true;
  }
  return false;
}

bool ConfigurationPolicyProviderWin::GetRegistryPolicyInteger(
    const wchar_t* value_name, uint32* result) {
  DWORD value;
  RegKey hkcu_policy_key(HKEY_LOCAL_MACHINE, kPolicyRegistrySubKey);
  if (hkcu_policy_key.ReadValueDW(value_name, &value)) {
    *result = value;
    return true;
  }

  RegKey hklm_policy_key(HKEY_CURRENT_USER, kPolicyRegistrySubKey);
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
              Value::CreateStringValueFromUTF16(string_value));
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

