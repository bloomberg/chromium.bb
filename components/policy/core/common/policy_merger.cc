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

PolicyListMerger::PolicyListMerger(
    const std::set<std::string> policies_to_merge)
    : policies_to_merge_(std::move(policies_to_merge)) {}
PolicyListMerger::~PolicyListMerger() = default;

void PolicyListMerger::Merge(PolicyMap::PolicyMapType* policies) const {
  DCHECK(policies);
  for (auto it = policies->begin(); it != policies->end(); it++) {
    if (CanMerge(it->first, it->second))
      (*policies)[it->first] = DoMerge(it->second);
  }
}

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

PolicyMap::Entry PolicyListMerger::DoMerge(
    const PolicyMap::Entry& policy) const {
  std::vector<const base::Value*> value;
  std::set<std::string> duplicates;
  bool merged = false;

  for (const base::Value& val : policy.value->GetList()) {
    if (duplicates.find(val.GetString()) != duplicates.end())
      continue;
    duplicates.insert(val.GetString());
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
    if (it.IsBlockedOrIgnored() ||
        it.source == POLICY_SOURCE_ENTERPRISE_DEFAULT || is_user_cloud_policy ||
        it.level != policy.level || it.scope != policy.scope) {
      continue;
    }

    for (const base::Value& val : it.value->GetList()) {
      if (duplicates.find(val.GetString()) != duplicates.end())
        continue;
      duplicates.insert(val.GetString());
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

PolicyGroupMerger::PolicyGroupMerger() = default;
PolicyGroupMerger::~PolicyGroupMerger() = default;

void PolicyGroupMerger::Merge(PolicyMap::PolicyMapType* policies) const {
  for (size_t i = 0; i < kPolicyAtomicGroupMappingsLength; ++i) {
    const AtomicGroup& group = kPolicyAtomicGroupMappings[i];
    bool use_highest_set_priority = false;

    // Defaults to the lowest priority.
    PolicyMap::Entry highest_set_priority;

    // Find the policy with the highest priority that is both in |policies| and
    // |group.policies|, an array ending with a nullptr.
    for (const char* const* policy_name = group.policies; *policy_name;
         ++policy_name) {
      auto policy_it = policies->find(*policy_name);

      if (policy_it == policies->end())
        continue;

      use_highest_set_priority = true;

      PolicyMap::Entry& policy = policy_it->second;

      if (!policy.has_higher_priority_than(highest_set_priority))
        continue;

      // Do not set POLICY_SOURCE_MERGED as the highest acceptable source
      // because it is a computed source. In case of an already merged policy,
      // the highest acceptable source must be the highest of the ones used to
      // compute the merged value.
      if (policy.source != POLICY_SOURCE_MERGED) {
        highest_set_priority = policy.DeepCopy();
      } else {
        for (const auto& conflict : policy.conflicts) {
          if (conflict.has_higher_priority_than(highest_set_priority) &&
              conflict.source > highest_set_priority.source) {
            highest_set_priority = conflict.DeepCopy();
          }
        }
      }
    }

    if (!use_highest_set_priority)
      continue;

    // Ignore the policies from |group.policies|, an array ending with a
    // nullptr, that do not share the same source as the one with the highest
    // priority.
    for (const char* const* policy_name = group.policies; *policy_name;
         ++policy_name) {
      auto policy_it = policies->find(*policy_name);
      if (policy_it == policies->end())
        continue;

      PolicyMap::Entry& policy = policy_it->second;

      if (policy.source < highest_set_priority.source)
        policy.SetIgnoredByPolicyAtomicGroup();
    }
  }
}

}  // namespace policy