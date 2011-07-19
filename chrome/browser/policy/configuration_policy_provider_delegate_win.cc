// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_provider_delegate_win.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "policy/policy_constants.h"

using base::win::RegKey;

namespace {

bool ReadRegistryStringValue(RegKey* key, const string16& name,
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

}  // namespace

namespace policy {

ConfigurationPolicyProviderDelegateWin::ConfigurationPolicyProviderDelegateWin(
    const ConfigurationPolicyProvider::PolicyDefinitionList*
        policy_definition_list)
    : policy_definition_list_(policy_definition_list) {
}

DictionaryValue* ConfigurationPolicyProviderDelegateWin::Load() {
  DictionaryValue* result = new DictionaryValue();
  const ConfigurationPolicyProvider::PolicyDefinitionList::Entry* current;
  for (current = policy_definition_list_->begin;
       current != policy_definition_list_->end;
       ++current) {
    const string16 name(ASCIIToUTF16(current->name));
    switch (current->value_type) {
      case Value::TYPE_STRING: {
        string16 string_value;
        if (GetRegistryPolicyString(name, &string_value)) {
          result->SetString(current->name, string_value);
        }
        break;
      }
      case Value::TYPE_LIST: {
        scoped_ptr<ListValue> list_value(new ListValue);
        if (GetRegistryPolicyStringList(name, list_value.get()))
          result->Set(current->name, list_value.release());
        break;
      }
      case Value::TYPE_BOOLEAN: {
        bool bool_value;
        if (GetRegistryPolicyBoolean(name, &bool_value)) {
          result->SetBoolean(current->name, bool_value);
        }
        break;
      }
      case Value::TYPE_INTEGER: {
        uint32 int_value;
        if (GetRegistryPolicyInteger(name, &int_value)) {
          result->SetInteger(current->name, int_value);
        }
        break;
      }
      default:
        NOTREACHED();
    }
  }
  return result;
}

bool ConfigurationPolicyProviderDelegateWin::GetRegistryPolicyString(
    const string16& name, string16* result) const {
  RegKey policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
  // First try the global policy.
  if (ReadRegistryStringValue(&policy_key, name, result))
    return true;

  // Fall back on user-specific policy.
  if (policy_key.Open(HKEY_CURRENT_USER, kRegistrySubKey,
                      KEY_READ) != ERROR_SUCCESS)
    return false;
  return ReadRegistryStringValue(&policy_key, name, result);
}

bool ConfigurationPolicyProviderDelegateWin::GetRegistryPolicyStringList(
    const string16& key, ListValue* result) const {
  string16 path = string16(kRegistrySubKey);
  path += ASCIIToUTF16("\\") + key;
  RegKey policy_key;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, path.c_str(), KEY_READ) !=
      ERROR_SUCCESS) {
    // Fall back on user-specific policy.
    if (policy_key.Open(HKEY_CURRENT_USER, path.c_str(), KEY_READ) !=
        ERROR_SUCCESS)
      return false;
  }
  string16 policy_string;
  int index = 0;
  while (ReadRegistryStringValue(&policy_key, base::IntToString16(++index),
                                 &policy_string)) {
    result->Append(Value::CreateStringValue(policy_string));
  }
  return true;
}

bool ConfigurationPolicyProviderDelegateWin::GetRegistryPolicyBoolean(
    const string16& value_name, bool* result) const {
  uint32 local_result = 0;
  bool ret = GetRegistryPolicyInteger(value_name, &local_result);
  if (ret)
    *result = local_result != 0;
  return ret;
}

bool ConfigurationPolicyProviderDelegateWin::GetRegistryPolicyInteger(
    const string16& value_name, uint32* result) const {
  DWORD value = 0;
  RegKey policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
  if (policy_key.ReadValueDW(value_name.c_str(), &value) == ERROR_SUCCESS) {
    *result = value;
    return true;
  }

  if (policy_key.Open(HKEY_CURRENT_USER, kRegistrySubKey, KEY_READ) ==
      ERROR_SUCCESS) {
    if (policy_key.ReadValueDW(value_name.c_str(), &value) == ERROR_SUCCESS) {
      *result = value;
      return true;
    }
  }
  return false;
}

}  // namespace policy
