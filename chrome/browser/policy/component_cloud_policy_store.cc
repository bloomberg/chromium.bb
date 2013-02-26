// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/component_cloud_policy_store.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_validator.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/resource_cache.h"
#include "googleurl/src/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kValue[] = "Value";
const char kLevel[] = "Level";
const char kMandatory[] = "Mandatory";
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
};

const DomainConstants* GetDomainConstants(PolicyDomain domain) {
  for (size_t i = 0; i < arraysize(kDomains); ++i) {
    if (kDomains[i].domain == domain)
      return &kDomains[i];
  }
  return NULL;
}

const DomainConstants* GetDomainConstantsForType(const std::string& type) {
  for (size_t i = 0; i < arraysize(kDomains); ++i) {
    if (kDomains[i].policy_type == type)
      return &kDomains[i];
  }
  return NULL;
}

}  // namespace

ComponentCloudPolicyStore::Delegate::~Delegate() {}

ComponentCloudPolicyStore::ComponentCloudPolicyStore(
    Delegate* delegate,
    ResourceCache* cache,
    const std::string& username,
    const std::string& dm_token)
    : delegate_(delegate),
      cache_(cache),
      username_(username),
      dm_token_(dm_token) {}

ComponentCloudPolicyStore::~ComponentCloudPolicyStore() {
  DCHECK(CalledOnValidThread());
}

// static
bool ComponentCloudPolicyStore::SupportsDomain(PolicyDomain domain) {
  return GetDomainConstants(domain) != NULL;
}

// static
bool ComponentCloudPolicyStore::GetPolicyType(PolicyDomain domain,
                                              std::string* policy_type) {
  const DomainConstants* constants = GetDomainConstants(domain);
  if (constants)
    *policy_type = constants->policy_type;
  return constants != NULL;
}

// static
bool ComponentCloudPolicyStore::GetPolicyDomain(const std::string& policy_type,
                                                PolicyDomain* domain) {
  const DomainConstants* constants = GetDomainConstantsForType(policy_type);
  if (constants)
    *domain = constants->domain;
  return constants != NULL;
}

const std::string& ComponentCloudPolicyStore::GetCachedHash(
    const PolicyNamespace& ns) const {
  DCHECK(CalledOnValidThread());
  std::map<PolicyNamespace, std::string>::const_iterator it =
      cached_hashes_.find(ns);
  return it == cached_hashes_.end() ? EmptyString() : it->second;
}

void ComponentCloudPolicyStore::Load() {
  DCHECK(CalledOnValidThread());

  typedef std::map<std::string, std::string> ContentMap;

  // Load all cached policy protobufs for each domain.
  for (size_t domain = 0; domain < arraysize(kDomains); ++domain) {
    const DomainConstants& constants = kDomains[domain];
    ContentMap protos;
    cache_->LoadAllSubkeys(constants.proto_cache_key, &protos);
    for (ContentMap::iterator it = protos.begin(); it != protos.end(); ++it) {
      const std::string& id(it->first);
      PolicyNamespace ns(constants.domain, id);

      // Validate each protobuf.
      scoped_ptr<em::PolicyFetchResponse> proto(new em::PolicyFetchResponse);
      em::ExternalPolicyData payload;
      if (!proto->ParseFromString(it->second) ||
          !ValidateProto(
              proto.Pass(), constants.policy_type, id, &payload, NULL)) {
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
      } else {
        // The data for this proto couldn't be loaded or is corrupted.
        Delete(ns);
      }
    }
  }
}

bool ComponentCloudPolicyStore::Store(const PolicyNamespace& ns,
                                      const std::string& serialized_policy,
                                      const std::string& secure_hash,
                                      const std::string& data) {
  DCHECK(CalledOnValidThread());
  const DomainConstants* constants = GetDomainConstants(ns.domain);
  PolicyMap policy;
  // |serialized_policy| has already been validated; validate the data now.
  if (!constants || !ValidateData(data, secure_hash, &policy))
    return false;

  // Flush the proto and the data to the cache.
  cache_->Store(constants->proto_cache_key, ns.component_id, serialized_policy);
  cache_->Store(constants->data_cache_key, ns.component_id, data);
  // And expose the policy.
  policy_bundle_.Get(ns).Swap(&policy);
  cached_hashes_[ns] = secure_hash;
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

void ComponentCloudPolicyStore::Purge(PolicyDomain domain,
                                      const std::set<std::string>& keep) {
  DCHECK(CalledOnValidThread());
  const DomainConstants* constants = GetDomainConstants(domain);
  if (!constants)
    return;

  cache_->PurgeOtherSubkeys(constants->proto_cache_key, keep);
  cache_->PurgeOtherSubkeys(constants->data_cache_key, keep);

  // Stop serving policies for purged namespaces.
  bool purged_current_policies = false;
  for (PolicyBundle::const_iterator it = policy_bundle_.begin();
       it != policy_bundle_.end(); ++it) {
    if (it->first.domain == domain &&
        keep.find(it->first.component_id) == keep.end() &&
        !policy_bundle_.Get(it->first).empty()) {
      policy_bundle_.Get(it->first).Clear();
      purged_current_policies = true;
    }
  }

  // Purge cached hashes, so that those namespaces can be fetched again if the
  // policy state changes.
  std::map<PolicyNamespace, std::string>::iterator it = cached_hashes_.begin();
  while (it != cached_hashes_.end()) {
    if (it->first.domain == domain &&
        keep.find(it->first.component_id) == keep.end()) {
      std::map<PolicyNamespace, std::string>::iterator prev = it;
      ++it;
      cached_hashes_.erase(prev);
    } else {
      ++it;
    }
  }

  if (purged_current_policies)
    delegate_->OnComponentCloudPolicyStoreUpdated();
}

bool ComponentCloudPolicyStore::ValidatePolicy(
    scoped_ptr<em::PolicyFetchResponse> proto,
    PolicyNamespace* ns,
    em::ExternalPolicyData* payload) {
  em::PolicyData policy_data;
  if (!ValidateProto(
          proto.Pass(), std::string(), std::string(), payload, &policy_data)) {
    return false;
  }

  if (!policy_data.has_policy_type())
    return false;

  const DomainConstants* constants =
      GetDomainConstantsForType(policy_data.policy_type());
  if (!constants || !policy_data.has_settings_entity_id())
    return false;

  ns->domain = constants->domain;
  ns->component_id = policy_data.settings_entity_id();
  return true;
}

bool ComponentCloudPolicyStore::ValidateProto(
    scoped_ptr<em::PolicyFetchResponse> proto,
    const std::string& policy_type,
    const std::string& settings_entity_id,
    em::ExternalPolicyData* payload,
    em::PolicyData* policy_data) {
  scoped_ptr<ComponentCloudPolicyValidator> validator(
      ComponentCloudPolicyValidator::Create(proto.Pass()));
  validator->ValidateUsername(username_);
  validator->ValidateDMToken(dm_token_,
                             ComponentCloudPolicyValidator::DM_TOKEN_REQUIRED);
  if (!policy_type.empty())
    validator->ValidatePolicyType(policy_type);
  if (!settings_entity_id.empty())
    validator->ValidateSettingsEntityId(settings_entity_id);
  validator->ValidatePayload();
  // TODO(joaodasilva): validate signature.
  validator->RunValidation();
  if (!validator->success())
    return false;

  em::ExternalPolicyData* data = validator->payload().get();
  // The download URL must be empty, or must be a valid URL.
  // An empty download URL signals that this component doesn't have cloud
  // policy, or that the policy has been removed.
  if (data->has_download_url() && !data->download_url().empty()) {
    if (!GURL(data->download_url()).is_valid() ||
        !data->has_secure_hash() ||
        data->secure_hash().empty()) {
      return false;
    }
  } else if (data->has_secure_hash()) {
    return false;
  }

  if (payload)
    payload->Swap(validator->payload().get());
  if (policy_data)
    policy_data->Swap(validator->policy_data().get());
  return true;
}

bool ComponentCloudPolicyStore::ValidateData(
    const std::string& data,
    const std::string& secure_hash,
    PolicyMap* policy) {
  return base::SHA1HashString(data) == secure_hash && ParsePolicy(data, policy);
}

bool ComponentCloudPolicyStore::ParsePolicy(const std::string& data,
                                            PolicyMap* policy) {
  scoped_ptr<base::Value> json(base::JSONReader::Read(
      data, base::JSON_PARSE_RFC | base::JSON_DETACHABLE_CHILDREN));
  base::DictionaryValue* dict = NULL;
  if (!json || !json->GetAsDictionary(&dict))
    return false;

  // Each top-level key maps a policy name to its description.
  //
  // Each description is an object that contains the policy value under the
  // "Value" key. The optional "Level" key is either "Mandatory" (default) or
  // "Recommended".
  for (base::DictionaryValue::key_iterator it = dict->begin_keys();
       it != dict->end_keys(); ++it) {
    base::DictionaryValue* description = NULL;
    if (!dict->GetDictionary(*it, &description))
      return false;

    base::Value* value = NULL;
    if (!description->Remove(kValue, &value))
      return false;

    PolicyLevel level = POLICY_LEVEL_MANDATORY;
    std::string level_string;
    if (description->GetString(kLevel, &level_string) &&
        level_string == kRecommended) {
      level = POLICY_LEVEL_RECOMMENDED;
    }

    // If policy for components is ever used for device-level settings then
    // this must support a configurable scope; assuming POLICY_SCOPE_USER is
    // fine for now.
    policy->Set(*it, level, POLICY_SCOPE_USER, value);
  }

  return true;
}

}  // namespace policy
