// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_cache_base.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"

namespace policy {

// A thin ConfigurationPolicyProvider implementation sitting on top of
// CloudPolicyCacheBase for hooking up with ConfigurationPolicyPrefStore.
class CloudPolicyCacheBase::CloudPolicyProvider
    : public ConfigurationPolicyProvider {
 public:
  CloudPolicyProvider(const PolicyDefinitionList* policy_list,
                      CloudPolicyCacheBase* cache,
                      CloudPolicyCacheBase::PolicyLevel level)
      : ConfigurationPolicyProvider(policy_list),
        cache_(cache),
        level_(level) {}
  virtual ~CloudPolicyProvider() {}

  virtual bool Provide(ConfigurationPolicyStoreInterface* store) {
    if (level_ == POLICY_LEVEL_MANDATORY)
      ApplyPolicyMap(&cache_->mandatory_policy_, store);
    else if (level_ == POLICY_LEVEL_RECOMMENDED)
      ApplyPolicyMap(&cache_->recommended_policy_, store);
    return true;
  }

  virtual bool IsInitializationComplete() const {
    return cache_->initialization_complete_;
  }

  virtual void AddObserver(ConfigurationPolicyProvider::Observer* observer) {
    cache_->observer_list_.AddObserver(observer);
  }
  virtual void RemoveObserver(ConfigurationPolicyProvider::Observer* observer) {
    cache_->observer_list_.RemoveObserver(observer);
  }

 private:
  // The underlying policy cache.
  CloudPolicyCacheBase* cache_;
  // Policy level this provider will handle.
  CloudPolicyCacheBase::PolicyLevel level_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyProvider);
};

CloudPolicyCacheBase::CloudPolicyCacheBase()
    : initialization_complete_(false),
      is_unmanaged_(false) {
  managed_policy_provider_.reset(
      new CloudPolicyProvider(
          ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
          this,
          POLICY_LEVEL_MANDATORY));
  recommended_policy_provider_.reset(
      new CloudPolicyProvider(
          ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
          this,
          POLICY_LEVEL_RECOMMENDED));
}

CloudPolicyCacheBase::~CloudPolicyCacheBase() {
  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_, OnProviderGoingAway());
}

bool CloudPolicyCacheBase::SetPolicyInternal(
    const em::PolicyFetchResponse& policy,
    base::Time* timestamp,
    bool check_for_timestamp_validity) {
  DCHECK(CalledOnValidThread());
  bool initialization_was_not_complete = !initialization_complete_;
  is_unmanaged_ = false;
  PolicyMap mandatory_policy;
  PolicyMap recommended_policy;
  base::Time temp_timestamp;
  bool ok = DecodePolicyResponse(policy, &mandatory_policy, &recommended_policy,
                                 &temp_timestamp);
  if (!ok) {
    LOG(WARNING) << "Decoding policy data failed.";
    return false;
  }
  if (timestamp) {
    *timestamp = temp_timestamp;
  }
  if (check_for_timestamp_validity &&
      temp_timestamp > base::Time::NowFromSystemTime()) {
    LOG(WARNING) << "Rejected policy data, file is from the future.";
    return false;
  }

  const bool new_policy_differs =
      !mandatory_policy_.Equals(mandatory_policy) ||
      !recommended_policy_.Equals(recommended_policy);
  mandatory_policy_.Swap(&mandatory_policy);
  recommended_policy_.Swap(&recommended_policy);
  initialization_complete_ = true;

  if (new_policy_differs || initialization_was_not_complete) {
    FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                      observer_list_, OnUpdatePolicy());
  }
  return true;
}

void CloudPolicyCacheBase::SetUnmanagedInternal(const base::Time& timestamp) {
  is_unmanaged_ = true;
  initialization_complete_ = true;
  mandatory_policy_.Clear();
  recommended_policy_.Clear();
  last_policy_refresh_time_ = timestamp;

  FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                    observer_list_, OnUpdatePolicy());
}

ConfigurationPolicyProvider* CloudPolicyCacheBase::GetManagedPolicyProvider() {
  DCHECK(CalledOnValidThread());
  return managed_policy_provider_.get();
}

ConfigurationPolicyProvider*
    CloudPolicyCacheBase::GetRecommendedPolicyProvider() {
  DCHECK(CalledOnValidThread());
  return recommended_policy_provider_.get();
}

bool CloudPolicyCacheBase::DecodePolicyResponse(
    const em::PolicyFetchResponse& policy_response,
    PolicyMap* mandatory,
    PolicyMap* recommended,
    base::Time* timestamp) {
  std::string data = policy_response.policy_data();
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(data)) {
    LOG(WARNING) << "Failed to parse PolicyData protobuf.";
    return false;
  }
  if (timestamp) {
    *timestamp = base::Time::UnixEpoch() +
                 base::TimeDelta::FromMilliseconds(policy_data.timestamp());
  }
  return DecodePolicyData(policy_data, mandatory, recommended);
}

}  // namespace policy
