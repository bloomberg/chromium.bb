// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of group policy lookup code.

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/group_policy.h"
#include "chrome/browser/group_policy_settings.h"
#include "chrome/installer/util/browser_distribution.h"

namespace group_policy {

// In GetPolicySettingsRootKey(), this gets appended with:
// [Publisher Name]\\[App name]\\.  Thus, this should be
// SOFTWARE\\Policies\\Chromium\\Chromium\\ in the case of Chromium.
#define kPolicyKey L"SOFTWARE\\Policies\\"

bool g_has_hklm_policies = false;
bool g_has_hkcu_policies = false;
bool g_has_settings_initialized = false;

// Get the root registry location of group policy settings.
bool GetPolicySettingsRootKey(std::wstring* policy_key) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring app_name = dist->GetApplicationName();
  std::wstring publisher_name = dist->GetPublisherName();
  std::wstring separator = std::wstring(L"\\");
  policy_key->append(kPolicyKey);
  policy_key->append(app_name);
  policy_key->append(separator);
  policy_key->append(publisher_name);
  policy_key->append(separator);
  return true;
}

// TODO(gwilson): Add a method that will check if group policy settings have
// been updated since we last checked, so as not to require a restart.
bool HasGroupPolicySettings() {
  if (g_has_settings_initialized)
    return g_has_hkcu_policies || g_has_hklm_policies;
  std::wstring policy_key(L"");
  GetPolicySettingsRootKey(&policy_key);
  RegKey hkcu = RegKey(HKEY_CURRENT_USER, policy_key.c_str(), KEY_QUERY_VALUE);
  RegKey hklm = RegKey(HKEY_LOCAL_MACHINE, policy_key.c_str(), KEY_QUERY_VALUE);
  g_has_hkcu_policies = hkcu.Valid();
  g_has_hklm_policies = hklm.Valid();
  g_has_settings_initialized = true;
  return g_has_hkcu_policies || g_has_hklm_policies;
}

bool HasHkcuSettings() {
  if (!g_has_settings_initialized)
    HasGroupPolicySettings();
  return g_has_hkcu_policies;
}

bool HasHklmSettings() {
  if (!g_has_settings_initialized)
    HasGroupPolicySettings();
  return g_has_hklm_policies;
}

// Returns true iff the setting is currently controlled by group policy.
// If anyone adds a policy that can only be present in HKLM or HKCU but
// not both, this code will need to be updated to reflect that.
bool SettingBase::IsPolicyControlled() const {
  DCHECK(regvalue_ != NULL && regkey_ != NULL) <<
      "Setting not initialized - don't call at static init time!";

  if (!HasGroupPolicySettings())
    return false;

  std::wstring regkey_path(L"");
  DCHECK(GetPolicySettingsRootKey(&regkey_path));
  regkey_path.append(regkey_);

  if (HasHklmSettings()) {
    RegKey key;
    HRESULT hr;
    hr = key.Open(HKEY_LOCAL_MACHINE, regkey_path.c_str(), KEY_QUERY_VALUE);
    if (SUCCEEDED(hr) && key.ValueExists(regvalue_))
      return true;
  }

  if (HasHkcuSettings()) {
    RegKey key;
    HRESULT hr;
    hr = key.Open(HKEY_CURRENT_USER, regkey_path.c_str(), KEY_QUERY_VALUE);
    if (SUCCEEDED(hr) && key.ValueExists(regvalue_))
      return true;
  }

  return false;
}

// Gets the current policy setting as a wstring.
HRESULT SettingBase::GetSetting(std::wstring* value, bool* found) const {
  // Default to an empty value.
  value->clear();

  // Failure is perfectly normal here, but continue to check HKCU.
  std::wstring hklm_value(L"");
  bool hklm_found = false;
  if (HasHklmSettings())
    GetSettingFromTree(HKEY_LOCAL_MACHINE, &hklm_value, &hklm_found);

  std::wstring hkcu_value(L"");
  bool hkcu_found = false;
  if (HasHkcuSettings())
    GetSettingFromTree(HKEY_CURRENT_USER, &hkcu_value, &hkcu_found);

  switch (combine_) {
    case POLICYCOMBINE_PREFERMACHINE:
      if (hklm_found) {
        value->assign(hklm_value);
        *found = true;
      } else if (hkcu_found) {
        value->assign(hkcu_value);
        *found = true;
      }
      break;

    case POLICYCOMBINE_PREFERUSER:
      if (hkcu_found) {
        value->assign(hkcu_value);
        *found = true;
      } else if (hklm_found) {
        value->assign(hklm_value);
        *found = true;
      }
      break;

    case POLICYCOMBINE_CONCATENATE:
      if (hklm_found) {
        value->assign(hklm_value);
        *found = true;
      }
      if (hkcu_found) {
        value->append(hkcu_value);
        *found = true;
      }
      break;

    default:
      DCHECK(FALSE) << "Unimplemented";
      NOTREACHED();
      break;
  }

  return S_OK;
}

// Gets the current policy setting as a ListVale of StringValue objects.
HRESULT SettingBase::GetSetting(ListValue* list, bool* found) const {
  if (regvalue_ == NULL || regkey_ == NULL)
    return E_UNEXPECTED;

  // Failure is perfectly normal here, but continue to check HKCU.
  scoped_ptr<ListValue> hklm_value(new ListValue());
  bool hklm_found = false;
  if (HasHklmSettings())
    GetSettingFromTree(HKEY_LOCAL_MACHINE, hklm_value.get(), &hklm_found);

  scoped_ptr<ListValue> hkcu_value(new ListValue());
  bool hkcu_found = false;
  if (HasHkcuSettings())
    GetSettingFromTree(HKEY_CURRENT_USER, hkcu_value.get(), &hkcu_found);

  // TODO(gwilson): Optimize / refactor this algorithm so that all of the
  // GetSetting() methods only read from the keys they must, rather than
  // reading from both HKCU and HKLM now.
  switch (combine_) {
    case POLICYCOMBINE_PREFERMACHINE:
      if (hklm_found) {
        AppendAll(list, hklm_value.get());
        *found = true;
      } else if (hkcu_found) {
        AppendAll(list, hkcu_value.get());
        *found = true;
      }
      break;

    case POLICYCOMBINE_PREFERUSER:
      if (hkcu_found) {
        AppendAll(list, hkcu_value.get());
        *found = true;
      } else if (hklm_found) {
        AppendAll(list, hklm_value.get());
        *found = true;
      }
      break;

    case POLICYCOMBINE_CONCATENATE:
      if (hklm_found) {
        AppendAll(list, hklm_value.get());
        *found = true;
      }
      if (hkcu_found) {
        AppendAll(list, hkcu_value.get());
        *found = true;
      }
      break;

    default:
      DCHECK(FALSE) << "Unimplemented";
      NOTREACHED();
      break;
  }

  return S_OK;
}

void AppendAll(ListValue* target, ListValue* source) {
  DCHECK(source);
  DCHECK(target);
  for (size_t i = 0; i < source->GetSize(); ++i) {
    // The string must be copied here, since the Value object in the
    // source list gets destroyed before we return the target list.
    std::string str;
    source->GetString(i, &str);
    target->Append(Value::CreateStringValue(str));
  }
}

// Gets the current policy setting as a bool.
HRESULT SettingBase::GetSetting(bool* value, bool* found) const {
  // Default to false.
  *value = false;

  // Failure is perfectly normal here, but continue to check HKCU.
  bool hklm_value = false;
  bool hklm_found = false;
  if (HasHklmSettings())
    GetSettingFromTree(HKEY_LOCAL_MACHINE, &hklm_value, &hklm_found);

  bool hkcu_value = false;
  bool hkcu_found = false;
  if (HasHkcuSettings())
    GetSettingFromTree(HKEY_CURRENT_USER, &hkcu_value, &hkcu_found);

  switch (combine_) {
    case POLICYCOMBINE_PREFERMACHINE:
      if (hklm_found) {
        *value = hklm_value;
        *found = true;
      } else if (hkcu_found) {
        *value = hkcu_value;
        *found = true;
      }
      break;

    case POLICYCOMBINE_PREFERUSER:
      if (hkcu_found) {
        *value = hkcu_value;
        *found = true;
      } else if (hklm_found) {
        *value = hklm_value;
        *found = true;
      }
      break;

    case POLICYCOMBINE_LOGICALOR:
      *value = hkcu_value || hklm_value;
      *found = hkcu_found || hklm_found;
      break;

    default:
      DCHECK(FALSE) << "Unimplemented";
      NOTREACHED();
      break;
  }

  return S_OK;
}

// Gets the current policy setting as a DWORD.
HRESULT SettingBase::GetSetting(DWORD* value, bool* found) const {
  // Default to zero.
  *value = 0;

  // Failure is perfectly normal here, but continue to check HKCU.
  DWORD hklm_value = 0;
  bool hklm_found = false;
  if (HasHklmSettings())
    GetSettingFromTree(HKEY_LOCAL_MACHINE, &hklm_value, &hklm_found);

  DWORD hkcu_value = 0;
  bool hkcu_found = false;
  if (HasHkcuSettings())
    GetSettingFromTree(HKEY_CURRENT_USER, &hkcu_value, &hkcu_found);

  switch (combine_) {
    case POLICYCOMBINE_PREFERMACHINE:
      if (hklm_found) {
        *value = hklm_value;
        *found = true;
      } else if (hkcu_found) {
        *value = hkcu_value;
        *found = true;
      }
      break;

    case POLICYCOMBINE_PREFERUSER:
      if (hkcu_found) {
        *value = hkcu_value;
        *found = true;
      } else if (hklm_found) {
        *value = hklm_value;
        *found = true;
      }
      break;

    case POLICYCOMBINE_LOGICALOR:
      *value = hkcu_value || hklm_value;
      *found = hkcu_found || hklm_found;
      break;

    default:
      DCHECK(FALSE) << "Unimplemented";
      NOTREACHED();
      break;
  }

  return S_OK;
}

// Gets the current policy setting in the specified tree.
HRESULT SettingBase::GetSettingFromTree(HKEY tree, DWORD* value,
                                        bool* found) const {
  *found = false;

  std::wstring reg_value_name(L"");
  DCHECK(GetPolicySettingsRootKey(&reg_value_name));
  reg_value_name.append(regkey_);

  HRESULT hr;
  RegKey key;
  // Failure is perfectly normal here.
  hr = key.Open(tree, reg_value_name.c_str(), KEY_QUERY_VALUE);

  if (FAILED(hr))
    return hr;

  DCHECK(storage_ == POLICYSTORAGE_SINGLEVALUE);
  hr = key.ReadValueDW(regvalue_, value);
  if (FAILED(hr))
    return hr;

  *found = true;

  return hr;
}

// Gets the current policy setting in the specified tree.
HRESULT SettingBase::GetSettingFromTree(HKEY tree, ListValue* list,
                                        bool* found) const {
  *found = false;
  list->Clear();

  std::wstring reg_value_name(L"");
  DCHECK(GetPolicySettingsRootKey(&reg_value_name));
  reg_value_name.append(regkey_);

  HRESULT hr;
  RegKey key;
  // Failure is perfectly normal here.
  hr = key.Open(tree,
                reg_value_name.c_str(),
                KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS);
  if (FAILED(hr))
    return hr;

  std::wstring reg_value;
  switch (storage_) {
    case POLICYSTORAGE_SINGLEVALUE:
      hr = key.ReadValue(regvalue_, &reg_value);
      if (FAILED(hr))
        return hr;
      list->Append(list->CreateStringValue(reg_value));
      *found = true;
      break;

    case POLICYSTORAGE_CONCATSUBKEYS: {
      int key_count = key.ValueCount();
      if (key_count == 0) {
        hr = E_FAIL;
      } else {
        // We'll return S_OK if we can succesfully read any of the subkeys.
        hr = E_FAIL;
        RegistryValueIterator iterator(tree, reg_value_name.c_str());
        for (; iterator.Valid(); ++iterator) {
          if (SUCCEEDED(key.ReadValue(iterator.Name(), &reg_value))) {
            list->Append(Value::CreateStringValue(reg_value.c_str()));
            *found = true;
            hr = S_OK;
          }
        }
      }
    }
    break;
  }
  return hr;
}

// Gets the current policy setting in the specified tree.
HRESULT SettingBase::GetSettingFromTree(HKEY tree, std::wstring* value,
                                        bool* found) const {
  *found = false;
  value->clear();

  std::wstring reg_value_name(L"");
  DCHECK(GetPolicySettingsRootKey(&reg_value_name));
  reg_value_name.append(regkey_);

  HRESULT hr;
  RegKey key;
  // Failure is perfectly normal here.
  hr = key.Open(tree, reg_value_name.c_str(), KEY_QUERY_VALUE);
  if (FAILED(hr))
    return hr;

  DCHECK(storage_ == POLICYSTORAGE_SINGLEVALUE);

  hr = key.ReadValue(regvalue_, value);
  if (FAILED(hr))
    return hr;

  *found = true;

  return hr;
}

// Gets the current policy setting in the specified tree.
HRESULT SettingBase::GetSettingFromTree(HKEY tree, bool* value,
                                        bool* found) const {
  *found = false;

  std::wstring reg_value_name(L"");
  DCHECK(GetPolicySettingsRootKey(&reg_value_name));
  reg_value_name.append(regkey_);

  HRESULT hr;
  RegKey key;
  // Failure is perfectly normal here.
  hr = key.Open(tree, reg_value_name.c_str(), KEY_QUERY_VALUE);
  if (FAILED(hr))
    return hr;

  DCHECK(storage_ == POLICYSTORAGE_SINGLEVALUE);
  DWORD full_value;
  hr = key.ReadValueDW(regvalue_, &full_value);
  if (FAILED(hr))
    return hr;

  *value = (full_value != 0);
  *found = true;

  return hr;
}

bool IsBoolOptionSet(const Setting<bool>& setting) {
  bool found = false;
  bool set = false;
  HRESULT hr = setting.GetSetting(&set, &found);
  if (FAILED(hr) || !found) {
    set = false;
  }
  return set;
}

};  // namespace group_policy

