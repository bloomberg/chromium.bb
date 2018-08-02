// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"

#include "base/stl_util.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/system_util.h"

namespace chrome_cleaner {
namespace {

const int kExtensionIdLength = 32;

struct RegistryKey {
  HKEY hkey;
  const wchar_t* path;
};

const RegistryKey extension_forcelist_keys[] = {
    {HKEY_LOCAL_MACHINE,
     L"software\\policies\\google\\chrome\\ExtensionInstallForcelist"},
    {HKEY_CURRENT_USER,
     L"software\\policies\\google\\chrome\\ExtensionInstallForcelist"}};

void GetForcelistPoliciesForAccessMask(
    REGSAM access_mask,
    std::vector<ExtensionRegistryPolicy>* policies) {
  for (size_t i = 0; i < base::size(extension_forcelist_keys); ++i) {
    base::win::RegistryValueIterator forcelist_it(
        extension_forcelist_keys[i].hkey, extension_forcelist_keys[i].path,
        access_mask);
    for (; forcelist_it.Valid(); ++forcelist_it) {
      base::string16 entry;
      GetRegistryValueAsString(forcelist_it.Value(), forcelist_it.ValueSize(),
                               forcelist_it.Type(), &entry);

      // Extract the extension ID from the beginning of the registry entry,
      // since it also contains an update URL.
      if (entry.length() >= kExtensionIdLength) {
        base::string16 extension_id = entry.substr(0, kExtensionIdLength);

        policies->emplace_back(extension_id, extension_forcelist_keys[i].hkey,
                               extension_forcelist_keys[i].path,
                               forcelist_it.Name());
      }
    }
  }
}

}  // namespace

ExtensionRegistryPolicy::ExtensionRegistryPolicy(
    const base::string16& extension_id,
    HKEY hkey,
    const base::string16& path,
    const base::string16& name)
    : extension_id(extension_id), hkey(hkey), path(path), name(name) {}

ExtensionRegistryPolicy::ExtensionRegistryPolicy(ExtensionRegistryPolicy&&) =
    default;

ExtensionRegistryPolicy& ExtensionRegistryPolicy::operator=(
    ExtensionRegistryPolicy&&) = default;

void GetExtensionForcelistRegistryPolicies(
    std::vector<ExtensionRegistryPolicy>* policies) {
  GetForcelistPoliciesForAccessMask(KEY_WOW64_32KEY, policies);
  if (IsX64Architecture())
    GetForcelistPoliciesForAccessMask(KEY_WOW64_64KEY, policies);
}

}  // namespace chrome_cleaner
