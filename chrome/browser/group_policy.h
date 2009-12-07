// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A central repository for definitions relating to Group Policy.

#ifndef CHROME_BROWSER_GROUP_POLICY_H_
#define CHROME_BROWSER_GROUP_POLICY_H_

#include <windows.h>
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"

// Each policy:
//   - has a name; this is used to refer to the policy for testing in code.
//   - has a type; the type of a policy decides what sort of value a policy
//         lookup returns as well as how the policy handles it when it is
//         defined both in user and machine policy.
//         Policy types include:
//           # Flag; a boolean flag, if either machine or user instance
//               of the flag is nonzero, then the flag is set. Otherwise,
//               whether both or either are nonexistent or zero, the flag
//               is unset.
//           # Number; a DWORD number.
//           # String; a character string.
//           # String List; an unordered list of strings.
//   - belongs to class; Each policy is either a machine or a user policy
//          or both.
//         Policies that belong to the user class are stored under HKCU, while
//         those of machine class are stored under HKLM. Policies of class both
//         belong in both places. For such policies, the meaning of
//         conflicting settings is determined by the type of the policy.
//   - has a registry key; This is a path from the toplevel key or keys that
//         the policy class implies.
//   - has a value name; Depending on the type of the policy, this is the name
//         of the value or the subkey that contains the policy flag or value.
//   - has a display name; this is the name displayed to the administrator in
//         the group policy editor.
//   - has a help string; this is the help string displayed to the administrator
//         in the group policy editor.

namespace group_policy {

// Get the root registry location of group policy settings.
bool GetPolicySettingsRootKey(std::wstring* policy_key);

// Check if the machine has any group policy settings.
bool HasGroupPolicySettings();

// True if the machine has group policy settings in HKCU.
bool HasHkcuSettings();

// True if the machine has group policy settings in HKLM.
bool HasHklmSettings();

// This specifies how to combine the user and machine keys for the
// policy setting; some are only applicable to certain data types
// and/or certain storage types.  Add more as needed.
enum PolicyCombine {
  POLICYCOMBINE_PREFERMACHINE,    // HKLM policy takes priority.
  POLICYCOMBINE_PREFERUSER,       // HKCU policy takes priority.
  POLICYCOMBINE_CONCATENATE,      // Concatenate both policies.
  POLICYCOMBINE_LOGICALOR,        // Logical OR of both keys.
};

// This specifies how the policy setting is stored in the registry.
// Add more as needed.
enum PolicyStorage {
  POLICYSTORAGE_SINGLEVALUE,    // Policy is a single registry value.
  POLICYSTORAGE_CONCATSUBKEYS,  // Policy is set of subkeys of specified key.
};

// Class that represents a group policy setting.  Encapsulates the lookup
// from both policy sections of the registry, as well as the method for
// combining values in the two policy sections.
class SettingBase {
 public:
  SettingBase(const wchar_t* regkey,
              const wchar_t* regvalue,
              PolicyStorage storage,
              PolicyCombine combine) : regkey_(regkey),
                  regvalue_(regvalue), storage_(storage),
                  combine_(combine) {
    // Both parameters are required.  Either can be empty (but not both.)
    DCHECK(regkey != NULL);
    DCHECK(regvalue != NULL);
    DCHECK(regkey[0] != L'\0' || regvalue[0] != L'\0');

    // If storage is POLICYSTORAGE_CONCATSUBKEYS, regvalue should be empty.
    DCHECK((storage != POLICYSTORAGE_CONCATSUBKEYS) ||
           regvalue[0] == L'\0');
  }

  // Returns whether this setting is controlled by group policy;
  // indicates whether this value is set somewhere in the registry.
  bool IsPolicyControlled() const;

 protected:
  // These accessors are protected and then individually exposed in a
  // templated base class.  That lets the compiler enforce that the code
  // using a policy is using the appropriate data type.
  HRESULT GetSetting(std::wstring* value, bool* found) const;
  HRESULT GetSetting(ListValue* list, bool* found) const;
  HRESULT GetSetting(DWORD* value, bool* found) const;
  HRESULT GetSetting(bool* value, bool* found) const;
  const wchar_t* regkey_;
  const wchar_t* regvalue_;

 private:
  // Private helper accessors for pulling individual registry keys
  HRESULT GetSettingFromTree(HKEY tree, std::wstring* value, bool* found) const;
  HRESULT GetSettingFromTree(HKEY tree, ListValue* value, bool* found) const;
  HRESULT GetSettingFromTree(HKEY tree, DWORD* value, bool* found) const;
  HRESULT GetSettingFromTree(HKEY tree, bool* value, bool* found) const;

  PolicyStorage storage_;
  PolicyCombine combine_;
};

// Templated class to provide type-safe access to policy setting.
template <typename T>
class Setting : public SettingBase {
 public:
  Setting(const wchar_t* regkey,
          const wchar_t* regvalue,
          PolicyStorage storage,
          PolicyCombine combine) :
      SettingBase(regkey, regvalue, storage, combine) {
  }

  HRESULT GetSetting(T* value, bool* found) const {
    DCHECK(found);
    DCHECK(value);
    DCHECK(regvalue_ != NULL && regkey_ != NULL) <<
        "Setting not initialized - don't call at static init time!";

    *found = false;

    if (!HasGroupPolicySettings())
      return S_OK;

    return SettingBase::GetSetting(value, found);
  }
};

// Is option set, default is 'no'.
bool IsBoolOptionSet(const Setting<bool>& setting);

// Helper function for appending all of one ListValue to another.
void AppendAll(ListValue* target, ListValue* source);

}  // namespace

#endif  // CHROME_BROWSER_GROUP_POLICY_H_

