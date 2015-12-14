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
#include "components/variations/variations_associated_data.h"

namespace startup_metric_utils {

namespace {

// Name of the pre-read field trial. This name is used to get the pre-read group
// to use for the next startup from base::FieldTrialList.
const char kPreReadFieldTrialName[] = "PreRead";

// Name of the synthetic pre-read field trial. A synthetic field trial is
// registered so that UMA metrics recorded during the current startup are
// annotated with the pre-read group that is actually used during this startup.
const char kPreReadSyntheticFieldTrialName[] = "SyntheticPreRead";

// Variation names for the PreRead field trial.
const base::char16 kNoPreReadVariationName[] = L"NoPreRead";
const base::char16 kHighPriorityVariationName[] = L"HighPriority";
const base::char16 kOnlyIfColdVariationName[] = L"OnlyIfCold";
const base::char16 kPrefetchVirtualMemoryVariationName[] =
    L"PrefetchVirtualMemory";

// Registry key in which the PreRead field trial group is stored.
const base::char16 kPreReadFieldTrialRegistryKey[] = L"\\PreReadFieldTrial";

// Returns the registry path in which the PreRead group is stored.
base::string16 GetPreReadRegistryPath(
    const base::string16& product_registry_path) {
  return product_registry_path + kPreReadFieldTrialRegistryKey;
}

}  // namespace

void GetPreReadOptions(const base::string16& product_registry_path,
                       bool* no_pre_read,
                       bool* high_priority,
                       bool* only_if_cold,
                       bool* prefetch_virtual_memory) {
  DCHECK(!product_registry_path.empty());
  DCHECK(no_pre_read);
  DCHECK(high_priority);
  DCHECK(only_if_cold);
  DCHECK(prefetch_virtual_memory);

  // Open the PreRead field trial's registry key.
  const base::string16 registry_path =
      GetPreReadRegistryPath(product_registry_path);
  const base::win::RegKey key(HKEY_CURRENT_USER, registry_path.c_str(),
                              KEY_QUERY_VALUE);

  // Set the PreRead field trial's options.
  struct VariationMapping {
    const base::char16* name;
    bool* variable;
  } const variations_mappings[] = {
      {kNoPreReadVariationName, no_pre_read},
      {kHighPriorityVariationName, high_priority},
      {kOnlyIfColdVariationName, only_if_cold},
      {kPrefetchVirtualMemoryVariationName, prefetch_virtual_memory},
  };

  for (const auto& mapping : variations_mappings) {
    // Set the option variable to true if the corresponding value is found in
    // the registry. Set to false otherwise (default behavior).
    DWORD value = 0;
    *mapping.variable = key.ReadValueDW(mapping.name, &value) == ERROR_SUCCESS;
    DCHECK(!*mapping.variable || value == 1);
  }
}

void UpdatePreReadOptions(const base::string16& product_registry_path) {
  DCHECK(!product_registry_path.empty());

  const base::string16 registry_path =
      GetPreReadRegistryPath(product_registry_path);

  // Clear the experiment key. That way, when the trial is shut down, the
  // registry will be cleaned automatically. Also, this prevents variation
  // params from the previous group to stay in the registry when the group is
  // updated.
  base::win::RegKey(HKEY_CURRENT_USER, registry_path.c_str(), KEY_SET_VALUE)
      .DeleteKey(L"");

  // Get the new group name.
  const base::string16 group = base::UTF8ToUTF16(
      base::FieldTrialList::FindFullName(kPreReadFieldTrialName));
  if (group.empty())
    return;

  // Get variation params for the new group.
  std::map<std::string, std::string> variation_params;
  variations::GetVariationParams(kPreReadFieldTrialName, &variation_params);

  // Open the registry key.
  base::win::RegKey key(HKEY_CURRENT_USER, registry_path.c_str(),
                        KEY_SET_VALUE);

  // Write variation params in the registry.
  for (const auto& variation_param : variation_params) {
    if (!variation_param.second.empty()) {
      if (key.WriteValue(base::UTF8ToUTF16(variation_param.first).c_str(), 1) !=
          ERROR_SUCCESS) {
        // Abort on failure to prevent the group name from being written in the
        // registry if not all variation params have been written. No synthetic
        // field trial is registered when there is no group name in the
        // registry.
        return;
      }
    }
  }

  // Write the new group name in the registry.
  key.WriteValue(L"", group.c_str());
}

void RegisterPreReadSyntheticFieldTrial(
    const base::string16& product_registry_path,
    const RegisterPreReadSyntheticFieldTrialCallback&
        register_synthetic_field_trial) {
  DCHECK(!product_registry_path.empty());

  const base::win::RegKey key(
      HKEY_CURRENT_USER, GetPreReadRegistryPath(product_registry_path).c_str(),
      KEY_QUERY_VALUE);
  base::string16 group;
  key.ReadValue(L"", &group);

  if (!group.empty()) {
    register_synthetic_field_trial.Run(kPreReadSyntheticFieldTrialName,
                                       base::UTF16ToUTF8(group));
  }
}

}  // namespace startup_metric_utils
