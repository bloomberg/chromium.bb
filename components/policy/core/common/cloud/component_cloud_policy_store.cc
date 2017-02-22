// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_store.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kValue[] = "Value";
const char kLevel[] = "Level";
const char kRecommended[] = "Recommended";

const struct DomainConstants {
  PolicyDomain domain;
  const char* proto_cache_key;
  const char* data_cache_key;
  const char* policy_type;
} kDomains[] = {
    {
        POLICY_DOMAIN_EXTENSIONS,
        "extension-policy",
        "extension-policy-data",
        dm_protocol::kChromeExtensionPolicyType,
    },
    {
        POLICY_DOMAIN_SIGNIN_EXTENSIONS,
        "signinextension-policy",
        "signinextension-policy-data",
        dm_protocol::kChromeSigninExtensionPolicyType,
    },
};

const DomainConstants* GetDomainConstants(PolicyDomain domain) {
  for (const DomainConstants& constants : kDomains) {
    if (constants.domain == domain)
      return &constants;
  }
  return nullptr;
}

const DomainConstants* GetDomainConstantsForType(const std::string& type) {
  for (const DomainConstants& constants : kDomains) {
    if (constants.policy_type == type)
      return &constants;
  }
  return nullptr;
}

base::Time GetTimeFromPolicyTimestamp(int64_t timestamp) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(timestamp);
}

}  // namespace

ComponentCloudPolicyStore::Delegate::~Delegate() {}

ComponentCloudPolicyStore::ComponentCloudPolicyStore(Delegate* delegate,
                                                     ResourceCache* cache)
    : delegate_(delegate), cache_(cache) {
  // Allow the store to be created on a different thread than the thread that
  // will end up using it.
  DetachFromThread();
}

ComponentCloudPolicyStore::~ComponentCloudPolicyStore() {
  DCHECK(CalledOnValidThread());
}

// static
bool ComponentCloudPolicyStore::SupportsDomain(PolicyDomain domain) {
  return GetDomainConstants(domain) != nullptr;
}

// static
bool ComponentCloudPolicyStore::GetPolicyType(PolicyDomain domain,
                                              std::string* policy_type) {
  const DomainConstants* constants = GetDomainConstants(domain);
  if (constants)
    *policy_type = constants->policy_type;
  return constants != nullptr;
}

// static
bool ComponentCloudPolicyStore::GetPolicyDomain(const std::string& policy_type,
                                                PolicyDomain* domain) {
  const DomainConstants* constants = GetDomainConstantsForType(policy_type);
  if (constants)
    *domain = constants->domain;
  return constants != nullptr;
}

const std::string& ComponentCloudPolicyStore::GetCachedHash(
    const PolicyNamespace& ns) const {
  DCHECK(CalledOnValidThread());
  std::map<PolicyNamespace, std::string>::const_iterator it =
      cached_hashes_.find(ns);
  return it == cached_hashes_.end() ? base::EmptyString() : it->second;
}

void ComponentCloudPolicyStore::SetCredentials(const std::string& username,
                                               const std::string& dm_token,
                                               const std::string& device_id,
                                               const std::string& public_key,
                                               int public_key_version) {
  DCHECK(CalledOnValidThread());
  DCHECK(username_.empty() || username == username_);
  DCHECK(dm_token_.empty() || dm_token == dm_token_);
  DCHECK(device_id_.empty() || device_id == device_id_);
  username_ = username;
  dm_token_ = dm_token;
  device_id_ = device_id;
  public_key_ = public_key;
  public_key_version_ = public_key_version;
}

void ComponentCloudPolicyStore::Load() {
  DCHECK(CalledOnValidThread());
  typedef std::map<std::string, std::string> ContentMap;

  // Load all cached policy protobufs for each domain.
  for (const DomainConstants& constants : kDomains) {
    ContentMap protos;
    cache_->LoadAllSubkeys(constants.proto_cache_key, &protos);
    for (ContentMap::iterator it = protos.begin(); it != protos.end(); ++it) {
      const std::string& id(it->first);
      const PolicyNamespace ns(constants.domain, id);

      // Validate each protobuf.
      std::unique_ptr<em::PolicyFetchResponse> proto(
          new em::PolicyFetchResponse);
      em::ExternalPolicyData payload;
      em::PolicyData policy_data;
      if (!proto->ParseFromString(it->second) ||
          !ValidatePolicy(ns, std::move(proto), &policy_data, &payload)) {
        Delete(ns);
        continue;
      }

      // The protobuf looks good; load the policy data.
      std::string data;
      PolicyMap policy;
      if (cache_->Load(constants.data_cache_key, id, &data) &&
          ValidateData(data, payload.secure_hash(), &policy)) {
        // The data is also good; expose the policies.
        policy_bundle_.Get(ns).Swap(&policy);
        cached_hashes_[ns] = payload.secure_hash();
        stored_policy_times_[ns] =
            GetTimeFromPolicyTimestamp(policy_data.timestamp());
      } else {
        // The data for this proto couldn't be loaded or is corrupted.
        Delete(ns);
      }
    }
  }
}

bool ComponentCloudPolicyStore::Store(
    const PolicyNamespace& ns,
    const std::string& serialized_policy,
    std::unique_ptr<em::PolicyData> policy_data,
    const std::string& secure_hash,
    const std::string& data) {
  DCHECK(CalledOnValidThread());
  const DomainConstants* constants = GetDomainConstants(ns.domain);
  PolicyMap policy;
  // |serialized_policy| has already been validated; validate the data now.
  if (!constants)
    return false;
  if (!ValidateData(data, secure_hash, &policy))
    return false;

  // Flush the proto and the data to the cache.
  cache_->Store(constants->proto_cache_key, ns.component_id, serialized_policy);
  cache_->Store(constants->data_cache_key, ns.component_id, data);
  // And expose the policy.
  policy_bundle_.Get(ns).Swap(&policy);
  cached_hashes_[ns] = secure_hash;
  stored_policy_times_[ns] =
      GetTimeFromPolicyTimestamp(policy_data->timestamp());
  delegate_->OnComponentCloudPolicyStoreUpdated();
  return true;
}

void ComponentCloudPolicyStore::Delete(const PolicyNamespace& ns) {
  DCHECK(CalledOnValidThread());
  const DomainConstants* constants = GetDomainConstants(ns.domain);
  if (!constants)
    return;

  cache_->Delete(constants->proto_cache_key, ns.component_id);
  cache_->Delete(constants->data_cache_key, ns.component_id);

  if (!policy_bundle_.Get(ns).empty()) {
    policy_bundle_.Get(ns).Clear();
    delegate_->OnComponentCloudPolicyStoreUpdated();
  }
}

void ComponentCloudPolicyStore::Purge(
    PolicyDomain domain,
    const ResourceCache::SubkeyFilter& filter) {
  DCHECK(CalledOnValidThread());
  const DomainConstants* constants = GetDomainConstants(domain);
  if (!constants)
    return;

  cache_->FilterSubkeys(constants->proto_cache_key, filter);
  cache_->FilterSubkeys(constants->data_cache_key, filter);

  // Stop serving policies for purged namespaces.
  bool purged_current_policies = false;
  for (PolicyBundle::const_iterator it = policy_bundle_.begin();
       it != policy_bundle_.end(); ++it) {
    if (it->first.domain == domain &&
        filter.Run(it->first.component_id) &&
        !policy_bundle_.Get(it->first).empty()) {
      policy_bundle_.Get(it->first).Clear();
      purged_current_policies = true;
    }
  }

  // Purge cached hashes, so that those namespaces can be fetched again if the
  // policy state changes.
  std::map<PolicyNamespace, std::string>::iterator it = cached_hashes_.begin();
  while (it != cached_hashes_.end()) {
    const PolicyNamespace ns(it->first);
    if (ns.domain == domain && filter.Run(ns.component_id)) {
      std::map<PolicyNamespace, std::string>::iterator prev = it;
      ++it;
      cached_hashes_.erase(prev);
      DCHECK(stored_policy_times_.count(ns));
      stored_policy_times_.erase(ns);
    } else {
      ++it;
    }
  }

  if (purged_current_policies)
    delegate_->OnComponentCloudPolicyStoreUpdated();
}

void ComponentCloudPolicyStore::Clear() {
  for (const DomainConstants& constants : kDomains) {
    cache_->Clear(constants.proto_cache_key);
    cache_->Clear(constants.data_cache_key);
  }
  cached_hashes_.clear();
  stored_policy_times_.clear();
  const PolicyBundle empty_bundle;
  if (!policy_bundle_.Equals(empty_bundle)) {
    policy_bundle_.Clear();
    delegate_->OnComponentCloudPolicyStoreUpdated();
  }
}

bool ComponentCloudPolicyStore::ValidatePolicy(
    const PolicyNamespace& ns,
    std::unique_ptr<em::PolicyFetchResponse> proto,
    em::PolicyData* policy_data,
    em::ExternalPolicyData* payload) {
  std::string policy_type;
  if (!GetPolicyType(ns.domain, &policy_type)) {
    LOG(ERROR) << "Bad policy type";
    return false;
  }
  if (ns.component_id.empty()) {
    LOG(ERROR) << "Empty component id";
    return false;
  }

  if (username_.empty() || dm_token_.empty() || device_id_.empty() ||
      public_key_.empty() || public_key_version_ == -1) {
    LOG(WARNING) << "Credentials are not loaded yet";
    return false;
  }

  // Calculate the bounds for the timestamp validation: a valid policy should be
  // not older than the currently stored policy, and also the timestamp should
  // not point too far in the future. This allows to prevent the rollback of the
  // policy, together with some protection against incorrectly large timestamps
  // that could be generated by the server due to some bug.
  base::Time time_not_before;
  const auto stored_policy_times_iter = stored_policy_times_.find(ns);
  if (stored_policy_times_iter != stored_policy_times_.end())
    time_not_before = stored_policy_times_iter->second;
  const base::Time time_not_after = base::Time::NowFromSystemTime();

  std::unique_ptr<ComponentCloudPolicyValidator> validator(
      ComponentCloudPolicyValidator::Create(
          std::move(proto), scoped_refptr<base::SequencedTaskRunner>()));
  validator->ValidateTimestamp(
      time_not_before, time_not_after,
      CloudPolicyValidatorBase::TIMESTAMP_FULLY_VALIDATED);
  validator->ValidateUsername(username_, true);
  validator->ValidateDMToken(dm_token_,
                             ComponentCloudPolicyValidator::DM_TOKEN_REQUIRED);
  validator->ValidateDeviceId(device_id_,
                              CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->ValidatePolicyType(policy_type);
  validator->ValidateSettingsEntityId(ns.component_id);
  validator->ValidatePayload();
  validator->ValidateSignature(public_key_);
  validator->RunValidation();
  if (!validator->success())
    return false;

  if (!validator->policy_data()->has_public_key_version()) {
    LOG(ERROR) << "Public key version missing";
    return false;
  }
  if (validator->policy_data()->public_key_version() != public_key_version_) {
    LOG(ERROR) << "Wrong public key version "
               << validator->policy_data()->public_key_version()
               << " - expected " << public_key_version_;
    return false;
  }

  em::ExternalPolicyData* data = validator->payload().get();
  // The download URL must be empty, or must be a valid URL.
  // An empty download URL signals that this component doesn't have cloud
  // policy, or that the policy has been removed.
  if (data->has_download_url() && !data->download_url().empty()) {
    if (!GURL(data->download_url()).is_valid()) {
      LOG(ERROR) << "Invalid URL: " << data->download_url();
      return false;
    }
    if (!data->has_secure_hash() || data->secure_hash().empty()) {
      LOG(ERROR) << "Secure hash missing";
      return false;
    }
  } else if (data->has_secure_hash()) {
    LOG(ERROR) << "URL missing";
    return false;
  }

  if (policy_data)
    policy_data->Swap(validator->policy_data().get());
  if (payload)
    payload->Swap(validator->payload().get());
  return true;
}

bool ComponentCloudPolicyStore::ValidateData(
    const std::string& data,
    const std::string& secure_hash,
    PolicyMap* policy) {
  return crypto::SHA256HashString(data) == secure_hash &&
      ParsePolicy(data, policy);
}

bool ComponentCloudPolicyStore::ParsePolicy(const std::string& data,
                                            PolicyMap* policy) {
  std::unique_ptr<base::Value> json = base::JSONReader::Read(
      data, base::JSON_PARSE_RFC | base::JSON_DETACHABLE_CHILDREN);
  base::DictionaryValue* dict = nullptr;
  if (!json || !json->GetAsDictionary(&dict))
    return false;

  // Each top-level key maps a policy name to its description.
  //
  // Each description is an object that contains the policy value under the
  // "Value" key. The optional "Level" key is either "Mandatory" (default) or
  // "Recommended".
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    base::DictionaryValue* description = nullptr;
    if (!dict->GetDictionaryWithoutPathExpansion(it.key(), &description))
      return false;

    std::unique_ptr<base::Value> value;
    if (!description->RemoveWithoutPathExpansion(kValue, &value))
      return false;

    PolicyLevel level = POLICY_LEVEL_MANDATORY;
    std::string level_string;
    if (description->GetStringWithoutPathExpansion(kLevel, &level_string) &&
        level_string == kRecommended) {
      level = POLICY_LEVEL_RECOMMENDED;
    }

    // If policy for components is ever used for device-level settings then
    // this must support a configurable scope; assuming POLICY_SCOPE_USER is
    // fine for now.
    policy->Set(it.key(), level, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                std::move(value), nullptr);
  }

  return true;
}

}  // namespace policy
