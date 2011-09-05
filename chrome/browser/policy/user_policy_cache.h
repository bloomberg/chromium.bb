// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_CACHE_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/user_policy_disk_cache.h"

namespace em = enterprise_management;

namespace enterprise_management {
class CachedCloudPolicyResponse;
// <Old-style policy support> (see comment below)
class GenericValue;
// </Old-style policy support>
}  // namespace enterprise_management

namespace policy {

// CloudPolicyCacheBase implementation that persists policy information
// into the file specified by the c'tor parameter |backing_file_path|.
class UserPolicyCache : public CloudPolicyCacheBase,
                        public UserPolicyDiskCache::Delegate {
 public:
  explicit UserPolicyCache(const FilePath& backing_file_path);
  virtual ~UserPolicyCache();

  // CloudPolicyCacheBase implementation:
  virtual void Load() OVERRIDE;
  virtual void SetPolicy(const em::PolicyFetchResponse& policy) OVERRIDE;
  virtual void SetUnmanaged() OVERRIDE;

 private:
  class DiskCache;

  // UserPolicyDiskCache::Delegate implementation:
  virtual void OnDiskCacheLoaded(
      UserPolicyDiskCache::LoadResult result,
      const em::CachedCloudPolicyResponse& cached_response) OVERRIDE;

  // CloudPolicyCacheBase implementation:
  virtual bool DecodePolicyData(const em::PolicyData& policy_data,
                                PolicyMap* mandatory,
                                PolicyMap* recommended) OVERRIDE;

  // <Old-style policy support>
  // The following member functions are needed to support old-style policy and
  // can be removed once all server-side components (CPanel, D3) have been
  // migrated to providing the new policy format.

  // If |mandatory| and |recommended| are both empty, and |policy_data|
  // contains a field named "repeated GenericNamedValue named_value = 2;",
  // this field is decoded into |mandatory|.
  void MaybeDecodeOldstylePolicy(const std::string& policy_data,
                                 PolicyMap* mandatory,
                                 PolicyMap* recommended);

  Value* DecodeIntegerValue(google::protobuf::int64 value) const;
  Value* DecodeValue(const em::GenericValue& value) const;

  // </Old-style policy support>

  // Manages the cache file.
  scoped_refptr<UserPolicyDiskCache> disk_cache_;

  // Used for constructing the weak ptr passed to |disk_cache_|.
  base::WeakPtrFactory<UserPolicyDiskCache::Delegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_CACHE_H_
