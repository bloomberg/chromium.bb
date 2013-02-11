// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_statistics_collector.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/common/pref_names.h"
#include "policy/policy_constants.h"

namespace policy {

const int PolicyStatisticsCollector::kStatisticsUpdateRate =
    24 * 60 * 60 * 1000;  // 24 hours.

PolicyStatisticsCollector::PolicyStatisticsCollector(
    PolicyService* policy_service,
    PrefService* prefs,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : max_policy_id_(-1),
      policy_service_(policy_service),
      prefs_(prefs),
      task_runner_(task_runner) {
}

PolicyStatisticsCollector::~PolicyStatisticsCollector() {
}

void PolicyStatisticsCollector::Initialize() {
  using base::Time;
  using base::TimeDelta;

  TimeDelta update_rate = TimeDelta::FromMilliseconds(kStatisticsUpdateRate);
  Time last_update = Time::FromInternalValue(
      prefs_->GetInt64(prefs::kLastPolicyStatisticsUpdate));
  TimeDelta delay = std::max(Time::Now() - last_update, TimeDelta::FromDays(0));
  if (delay >= update_rate)
    CollectStatistics();
  else
    ScheduleUpdate(update_rate - delay);
}

// static
void PolicyStatisticsCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kLastPolicyStatisticsUpdate, 0);
}

void PolicyStatisticsCollector::RecordPolicyUse(int id) {
  if (max_policy_id_ == -1) {
    const policy::PolicyDefinitionList* policy_list =
        policy::GetChromePolicyDefinitionList();
    for (const policy::PolicyDefinitionList::Entry* policy = policy_list->begin;
         policy != policy_list->end; ++policy) {
      if (policy->id > max_policy_id_)
        max_policy_id_ = policy->id;
    }
  }

  // Set the boundary to be max policy id + 1. Note that this may decrease in
  // future builds if the policy with maximum id is removed. This does not
  // pose a problem.
  DCHECK_LE(id, max_policy_id_);
  UMA_HISTOGRAM_ENUMERATION("Enterprise.Policies", id, max_policy_id_ + 1);
}

void PolicyStatisticsCollector::CollectStatistics() {
  const policy::PolicyDefinitionList* policy_list =
      policy::GetChromePolicyDefinitionList();
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  // Collect statistics.
  for (const policy::PolicyDefinitionList::Entry* policy = policy_list->begin;
       policy != policy_list->end; ++policy) {
    if (policies.Get(policy->name))
      RecordPolicyUse(policy->id);
  }

  // Take care of next update.
  prefs_->SetInt64(prefs::kLastPolicyStatisticsUpdate,
                   base::Time::Now().ToInternalValue());
  ScheduleUpdate(base::TimeDelta::FromMilliseconds(kStatisticsUpdateRate));
}

void PolicyStatisticsCollector::ScheduleUpdate(base::TimeDelta delay) {
  update_callback_.Reset(base::Bind(
      &PolicyStatisticsCollector::CollectStatistics,
      base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, update_callback_.callback(), delay);
}

}  // namespace policy
