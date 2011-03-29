// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_CACHE_H_
#pragma once

#include "base/file_path.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"

namespace policy {

// CloudPolicyCacheBase implementation that persists policy information
// into the file specified by the c'tor parameter |backing_file_path|.
class UserPolicyCache : public CloudPolicyCacheBase {
 public:
  explicit UserPolicyCache(const FilePath& backing_file_path);
  virtual ~UserPolicyCache();

  // CloudPolicyCacheBase implementation:
  virtual void Load() OVERRIDE;
  virtual void SetPolicy(const em::PolicyFetchResponse& policy) OVERRIDE;
  virtual void SetUnmanaged() OVERRIDE;

 private:
  void PersistPolicy(const em::PolicyFetchResponse& policy,
                     const base::Time& timestamp);

  // CloudPolicyCacheBase implementation:
  virtual bool DecodePolicyData(const em::PolicyData& policy_data,
                                PolicyMap* mandatory,
                                PolicyMap* recommended) OVERRIDE;

  // The file in which we store a cached version of the policy information.
  const FilePath backing_file_path_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_CACHE_H_
