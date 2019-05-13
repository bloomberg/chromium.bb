// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

PolicyMerger::PolicyMerger() = default;
PolicyMerger::~PolicyMerger() = default;

void PolicyMerger::Merge(PolicyMap::PolicyMapType* result) const {
  for (auto it = result->begin(); it != result->end(); it++) {
    if (CanMerge(it->first, it->second))
      (*result)[it->first] = Merge(it->second);
  }
}

PolicyListMerger::PolicyListMerger(
    const std::set<std::string> policies_to_merge)
    : policies_to_merge_(std::move(policies_to_merge)) {}
PolicyListMerger::~PolicyListMerger() = default;

bool PolicyListMerger::CanMerge(const std::string& policy_name,
                                PolicyMap::Entry& policy) const {
  if (policy.source == POLICY_SOURCE_MERGED)
    return false;

  if (policies_to_merge_.find("*") != policies_to_merge_.end())
    return policy.value->is_list();

  if (policies_to_merge_.find(policy_name) == policies_to_merge_.end())
    return false;

  if (!policy.value->is_list()) {
    policy.AddError(IDS_POLICY_LIST_MERGING_WRONG_POLICY_TYPE_SPECIFIED);
    return false;
  }

  return true;
}

PolicyMap::Entry PolicyListMerger::Merge(const PolicyMap::Entry& policy) const {
  std::vector<const base::Value*> value;
  auto compare_value_ptr = [](const base::Value* a, const base::Value* b) {
    return *a < *b;
  };
  std::set<const base::Value*, decltype(compare_value_ptr)> duplicates(
      compare_value_ptr);
  bool merged = false;

  for (const base::Value& val : policy.value->GetList()) {
    if (duplicates.find(&val) != duplicates.end())
      continue;
    duplicates.insert(&val);
    value.push_back(&val);
  }

  for (const auto& it : policy.conflicts) {
    // On desktop, the user cloud policy potentially comes from a different
    // domain than e.g. GPO policy or machine-level cloud policy, so prevent
    // merging user cloud policy with other policy sources.
    const bool is_user_cloud_policy =
        it.scope == POLICY_SCOPE_USER &&
        (it.source == POLICY_SOURCE_CLOUD ||
         it.source == POLICY_SOURCE_PRIORITY_CLOUD);
    if (it.IsBlocked() || it.source == POLICY_SOURCE_ENTERPRISE_DEFAULT ||
        is_user_cloud_policy || it.level != policy.level ||
        it.scope != policy.scope) {
      continue;
    }

    for (const base::Value& val : it.value->GetList()) {
      if (duplicates.find(&val) != duplicates.end())
        continue;
      duplicates.insert(&val);
      value.push_back(&val);
    }

    merged = true;
  }

  auto result = policy.DeepCopy();

  if (merged) {
    result.ClearConflicts();
    result.AddConflictingPolicy(policy);
    result.source = POLICY_SOURCE_MERGED;
    result.value.reset(new base::Value(base::Value::Type::LIST));
    for (const base::Value* it : value)
      result.value->GetList().emplace_back(it->Clone());
  }

  return result;
}

}  // namespace policy