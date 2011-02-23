// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_CACHE_H_

#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "policy/configuration_policy_type.h"

class DictionaryValue;
class ListValue;
class Value;

using google::protobuf::RepeatedPtrField;

namespace policy {

namespace em = enterprise_management;

// Keeps the authoritative copy of cloud policy information as read from the
// persistence file or determined by the policy backend. The cache doesn't talk
// to the service directly, but receives updated policy information through
// SetPolicy() calls, which is then persisted and decoded into the internal
// Value representation chrome uses.
class CloudPolicyCache : public base::NonThreadSafe {
 public:
  // Used to distinguish mandatory from recommended policies.
  enum PolicyLevel {
    // Policy is forced upon the user and should always take effect.
    POLICY_LEVEL_MANDATORY,
    // The value is just a recommendation that the user may override.
    POLICY_LEVEL_RECOMMENDED,
  };

  explicit CloudPolicyCache(const FilePath& backing_file_path);
  ~CloudPolicyCache();

  // Loads policy information from the backing file. Non-existing or erroneous
  // cache files are ignored.
  void LoadFromFile();

  // Resets the policy information.
  void SetPolicy(const em::CloudPolicyResponse& policy);
  void SetDevicePolicy(const em::DevicePolicyResponse& policy);

  ConfigurationPolicyProvider* GetManagedPolicyProvider();
  ConfigurationPolicyProvider* GetRecommendedPolicyProvider();

  void SetUnmanaged();
  bool is_unmanaged() const {
    return is_unmanaged_;
  }

  // Returns the time at which the policy was last fetched.
  base::Time last_policy_refresh_time() const {
    return last_policy_refresh_time_;
  }

  // Returns true if this cache holds (old-style) device policy that should be
  // given preference over (new-style) mandatory/recommended policy.
  bool has_device_policy() const {
    return has_device_policy_;
  }

 private:
  class CloudPolicyProvider;

  friend class CloudPolicyCacheTest;
  friend class DeviceManagementPolicyCacheTest;
  friend class DeviceManagementPolicyCacheDecodeTest;

  // Decodes a CloudPolicyResponse into two (ConfigurationPolicyType -> Value*)
  // maps and a timestamp. Also performs verification, returns NULL if any
  // check fails.
  static bool DecodePolicyResponse(
      const em::CloudPolicyResponse& policy_response,
      PolicyMap* mandatory,
      PolicyMap* recommended,
      base::Time* timestamp);

  // Returns true if |certificate_chain| is trusted and a |signature| created
  // from it matches |data|.
  static bool VerifySignature(
      const std::string& signature,
      const std::string& data,
      const RepeatedPtrField<std::string>& certificate_chain);

  // Decodes an int64 value. Checks whether the passed value fits the numeric
  // limits of the value representation. Returns a value (ownership is
  // transferred to the caller) on success, NULL on failure.
  static Value* DecodeIntegerValue(google::protobuf::int64 value);

  // Decode a GenericValue message to the Value representation used internally.
  // Returns NULL if |value| is invalid (i.e. contains no actual value).
  static Value* DecodeValue(const em::GenericValue& value);

  // Decodes a policy message and returns it in Value representation. Ownership
  // of the returned dictionary is transferred to the caller.
  static DictionaryValue* DecodeDevicePolicy(
      const em::DevicePolicyResponse& response);

  // The file in which we store a cached version of the policy information.
  const FilePath backing_file_path_;

  // Policy key-value information.
  PolicyMap mandatory_policy_;
  PolicyMap recommended_policy_;
  scoped_ptr<DictionaryValue> device_policy_;

  // Whether initialization has been completed. This is the case when we have
  // valid policy, learned that the device is unmanaged or ran into
  // unrecoverable errors.
  bool initialization_complete_;

  // Whether the the server has indicated this device is unmanaged.
  bool is_unmanaged_;

  // Tracks whether the cache currently stores |device_policy_| that should be
  // given preference over |mandatory_policy_| and |recommended_policy_|.
  bool has_device_policy_;

  // The time at which the policy was last refreshed.
  base::Time last_policy_refresh_time_;

  // Policy providers.
  scoped_ptr<ConfigurationPolicyProvider> managed_policy_provider_;
  scoped_ptr<ConfigurationPolicyProvider> recommended_policy_provider_;

  // Provider observers that are registered with this cache's providers.
  ObserverList<ConfigurationPolicyProvider::Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_CACHE_H_
