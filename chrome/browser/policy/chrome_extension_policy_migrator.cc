// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_extension_policy_migrator.h"
#include "extensions/common/hashed_extension_id.h"

namespace policy {

void ChromeExtensionPolicyMigrator::CopyPoliciesIfUnset(
    PolicyBundle* bundle,
    const std::string& hashed_extension_id,
    base::span<const Migration> migrations) {
  // HashedExtensionId gives an all-uppercase output, so make sure the input is
  // all uppercase.
  std::string hashed_extension_id_uppercase = hashed_extension_id;
  std::transform(hashed_extension_id_uppercase.begin(),
                 hashed_extension_id_uppercase.end(),
                 hashed_extension_id_uppercase.begin(), ::toupper);

  // Look for an extension with this hash.
  PolicyMap* extension_map = nullptr;
  for (auto& policy : *bundle) {
    const PolicyNamespace& policy_namespace = policy.first;

    if (policy_namespace.domain == PolicyDomain::POLICY_DOMAIN_EXTENSIONS &&
        extensions::HashedExtensionId(policy_namespace.component_id).value() ==
            hashed_extension_id_uppercase) {
      extension_map = policy.second.get();
      break;
    }
  }

  if (extension_map == nullptr || extension_map->empty())
    return;

  PolicyMap& chrome_map = bundle->Get(PolicyNamespace(
      PolicyDomain::POLICY_DOMAIN_CHROME, /* component_id */ std::string()));

  for (const auto& migration : migrations) {
    PolicyMap::Entry* entry = extension_map->GetMutable(migration.old_name);
    if (entry) {
      if (!chrome_map.Get(migration.new_name)) {
        chrome_map.Set(migration.new_name, entry->DeepCopy());
      }
      // TODO(crbug/869958): Mark the old policy as deprecated for
      // chrome://policy.
    }
  }
}

}  // namespace policy
