// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/pre_read_field_trial_utils_win.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace startup_metric_utils {

namespace {

// Name of the pre-read field trial. This name is used to get the pre-read group
// to use for the next startup from base::FieldTrialList.
const char kPreReadFieldTrialName[] = "PreRead";

// Name of the synthetic pre-read field trial. A synthetic field trial is
// registered so that UMA metrics recorded during the current startup are
// annotated with the pre-read group that is actually used during this startup.
const char kPreReadSyntheticFieldTrialName[] = "SyntheticPreRead";

// Group names for the PreRead field trial.
const base::char16 kPreReadDisabledGroupName[] = L"Disabled";
const base::char16 kPreReadEnabledHighPriorityGroupName[] =
    L"EnabledHighPriority";

// Registry key in which the PreRead field trial group is stored.
const base::char16 kPreReadFieldTrialRegistryKey[] = L"\\PreReadFieldTrial";

// Returns the registry path in which the PreRead group is stored.
base::string16 GetPreReadRegistryPath(
    const base::string16& product_registry_path) {
  return product_registry_path + kPreReadFieldTrialRegistryKey;
}

// Returns the PreRead group stored in the registry, or an empty string if no
// group is stored in the registry.
base::string16 GetPreReadGroup(const base::string16& product_registry_path) {
  const base::string16 registry_path =
      GetPreReadRegistryPath(product_registry_path);
  const base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_path.c_str(),
                                       KEY_QUERY_VALUE);
  base::string16 group;
  registry_key.ReadValue(L"", &group);
  return group;
}

}  // namespace

void GetPreReadOptions(const base::string16& product_registry_path,
                       bool* should_pre_read,
                       bool* should_pre_read_high_priority) {
  DCHECK(!product_registry_path.empty());
  DCHECK(should_pre_read);
  DCHECK(should_pre_read_high_priority);

  // Read the pre-read group from the registry.
  const base::string16 group = GetPreReadGroup(product_registry_path);

  // Set pre-read options according to the group.
  if (base::StartsWith(group, kPreReadDisabledGroupName,
                       base::CompareCase::SENSITIVE)) {
    *should_pre_read = false;
    *should_pre_read_high_priority = false;
  } else if (base::StartsWith(group, kPreReadEnabledHighPriorityGroupName,
                              base::CompareCase::SENSITIVE)) {
    *should_pre_read = true;
    *should_pre_read_high_priority = true;
  } else {
    // Default behavior.
    *should_pre_read = true;
    *should_pre_read_high_priority = false;
  }
}

void UpdatePreReadOptions(const base::string16& product_registry_path) {
  DCHECK(!product_registry_path.empty());

  const base::string16 registry_path =
      GetPreReadRegistryPath(product_registry_path);
  const std::string group =
      base::FieldTrialList::FindFullName(kPreReadFieldTrialName);
  if (group.empty()) {
    // Clear the registry key. That way, when the trial is shut down, the
    // registry will be cleaned automatically.
    base::win::RegKey(HKEY_CURRENT_USER, registry_path.c_str(), KEY_SET_VALUE)
        .DeleteKey(L"");
  } else {
    // Write the group in the registry.
    base::win::RegKey(HKEY_CURRENT_USER, registry_path.c_str(), KEY_SET_VALUE)
        .WriteValue(L"", base::UTF8ToUTF16(group).c_str());
  }
}

void RegisterPreReadSyntheticFieldTrial(
    const base::string16& product_registry_path,
    const RegisterPreReadSyntheticFieldTrialCallback&
        register_synthetic_field_trial) {
  DCHECK(!product_registry_path.empty());

  register_synthetic_field_trial.Run(
      kPreReadSyntheticFieldTrialName,
      base::UTF16ToUTF8(GetPreReadGroup(product_registry_path)));
}

}  // namespace startup_metric_utils
