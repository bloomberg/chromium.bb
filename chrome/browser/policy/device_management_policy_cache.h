// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_CACHE_H_

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

class DictionaryValue;
class Value;

namespace policy {

namespace em = enterprise_management;

// Keeps the authoritative copy of cloud policy information as read from the
// persistence file or determined by the policy backend. The cache doesn't talk
// to the service directly, but receives updated policy information through
// SetPolicy() calls, which is then persisted and decoded into the internal
// Value representation chrome uses.
class DeviceManagementPolicyCache {
 public:
  explicit DeviceManagementPolicyCache(const FilePath& backing_file_path);
  ~DeviceManagementPolicyCache();

  // Loads policy information from the backing file. Non-existing or erroneous
  // cache files are ignored.
  void LoadPolicyFromFile();

  // Resets the policy information. Returns true if the new policy is different
  // from the previously stored policy.
  bool SetPolicy(const em::DevicePolicyResponse& policy);

  // Gets the policy information. Ownership of the return value is transferred
  // to the caller.
  DictionaryValue* GetPolicy();

  void SetDeviceUnmanaged();
  bool is_device_unmanaged() const {
    return is_device_unmanaged_;
  }

  // Returns the time as which the policy was last fetched.
  base::Time last_policy_refresh_time() const {
    return last_policy_refresh_time_;
  }

 private:
  friend class DeviceManagementPolicyCacheDecodeTest;
  FRIEND_TEST_ALL_PREFIXES(DeviceManagementPolicyCacheDecodeTest, DecodePolicy);

  // Decodes an int64 value. Checks whether the passed value fits the numeric
  // limits of the value representation. Returns a value (ownership is
  // transferred to the caller) on success, NULL on failure.
  static Value* DecodeIntegerValue(google::protobuf::int64 value);

  // Decode a GenericValue message to the Value representation used internally.
  // Returns NULL if |value| is invalid (i.e. contains no actual value).
  static Value* DecodeValue(const em::GenericValue& value);

  // Decodes a policy message and returns it in Value representation. Ownership
  // of the returned dictionary is transferred to the caller.
  static DictionaryValue* DecodePolicy(
      const em::DevicePolicyResponse& response);

  // The file in which we store a cached version of the policy information.
  const FilePath backing_file_path_;

  // Protects |policy_|.
  base::Lock lock_;

  // Policy key-value information.
  scoped_ptr<DictionaryValue> policy_;

  // Tracks whether the store received a SetPolicy() call, which overrides any
  // information loaded from the file.
  bool fresh_policy_;

  bool is_device_unmanaged_;

  // The time at which the policy was last refreshed.
  base::Time last_policy_refresh_time_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_POLICY_CACHE_H_
