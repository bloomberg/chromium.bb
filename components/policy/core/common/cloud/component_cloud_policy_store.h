// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_STORE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace enterprise_management {
class ExternalPolicyData;
class PolicyData;
class PolicyFetchResponse;
}

namespace policy {

// Validates protobufs for external policy data, validates the data itself, and
// caches both locally.
class POLICY_EXPORT ComponentCloudPolicyStore : public base::NonThreadSafe {
 public:
  class POLICY_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Invoked whenever the policies served by policy() have changed, except
    // for the initial Load().
    virtual void OnComponentCloudPolicyStoreUpdated() = 0;
  };

  // Both the |delegate| and the |cache| must outlive this object.
  ComponentCloudPolicyStore(Delegate* delegate,
                            ResourceCache* cache);
  ~ComponentCloudPolicyStore();

  // Helper that returns true for PolicyDomains that can be managed by this
  // store.
  static bool SupportsDomain(PolicyDomain domain);

  // Returns true if |domain| can be managed by this store; in that case, the
  // dm_protocol policy type that corresponds to |domain| is stored in
  // |policy_type|. Otherwise returns false.
  static bool GetPolicyType(PolicyDomain domain, std::string* policy_type);

  // Returns true if |policy_type| corresponds to a policy domain that can be
  // managed by this store; in that case, the domain constants is assigned to
  // |domain|. Otherwise returns false.
  static bool GetPolicyDomain(const std::string& policy_type,
                              PolicyDomain* domain);

  // The current list of policies.
  const PolicyBundle& policy() const { return policy_bundle_; }

  // The cached hash for namespace |ns|, or the empty string if |ns| is not
  // cached.
  const std::string& GetCachedHash(const PolicyNamespace& ns) const;

  // |username| and |dm_token| are used to validate the cached data, and data
  // stored later.
  // All ValidatePolicy() requests without credentials fail.
  void SetCredentials(const std::string& username,
                      const std::string& dm_token);

  // Loads and validates all the currently cached protobufs and policy data.
  // This is performed synchronously, and policy() will return the cached
  // policies after this call.
  void Load();

  // Stores the protobuf and |data| for namespace |ns|. The protobuf is passed
  // serialized in |serialized_policy_proto|, and must have been validated
  // before.
  // The |data| is validated during this call, and its secure hash must match
  // |secure_hash|.
  // Returns false if |data| failed validation, otherwise returns true and the
  // data was stored in the cache.
  bool Store(const PolicyNamespace& ns,
             const std::string& serialized_policy_proto,
             const std::string& secure_hash,
             const std::string& data);

  // Deletes the storage of namespace |ns| and stops serving its policies.
  void Delete(const PolicyNamespace& ns);

  // Deletes the storage of all components of |domain| that pass then given
  // |filter|, and stops serving their policies.
  void Purge(PolicyDomain domain,
             const ResourceCache::SubkeyFilter& filter);

  // Deletes the storage of every component.
  void Clear();

  // Validates |proto| and returns the corresponding policy namespace in |ns|,
  // and the parsed ExternalPolicyData in |payload|.
  // If |proto| validates successfully then its |payload| can be trusted, and
  // the data referenced there can be downloaded. A |proto| must be validated
  // before attempting to download the data, and before storing both.
  bool ValidatePolicy(
      scoped_ptr<enterprise_management::PolicyFetchResponse> proto,
      PolicyNamespace* ns,
      enterprise_management::ExternalPolicyData* payload);

 private:
  // Helper for ValidatePolicy(), that's also used to validate protobufs
  // loaded from the disk cache.
  bool ValidateProto(
      scoped_ptr<enterprise_management::PolicyFetchResponse> proto,
      const std::string& policy_type,
      const std::string& settings_entity_id,
      enterprise_management::ExternalPolicyData* payload,
      enterprise_management::PolicyData* policy_data);

  // Validates the JSON policy serialized in |data|, and verifies its hash
  // with |secure_hash|. Returns true on success, and in that case stores the
  // parsed policies in |policy|.
  bool ValidateData(const std::string& data,
                    const std::string& secure_hash,
                    PolicyMap* policy);

  // Parses the JSON policy in |data| into |policy|, and returns true if the
  // parse was successful.
  bool ParsePolicy(const std::string& data, PolicyMap* policy);

  Delegate* delegate_;
  ResourceCache* cache_;
  std::string username_;
  std::string dm_token_;

  PolicyBundle policy_bundle_;
  std::map<PolicyNamespace, std::string> cached_hashes_;

  DISALLOW_COPY_AND_ASSIGN(ComponentCloudPolicyStore);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_STORE_H_
