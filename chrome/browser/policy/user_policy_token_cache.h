// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_TOKEN_CACHE_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_TOKEN_CACHE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"

namespace policy {

// Responsible for managing the on-disk token cache.
class UserPolicyTokenCache
    : public base::RefCountedThreadSafe<UserPolicyTokenCache>,
      public CloudPolicyDataStore::Observer {
 public:
  UserPolicyTokenCache(const base::WeakPtr<CloudPolicyDataStore>& data_store,
                       const FilePath& cache_file);

  // Starts loading the disk cache. After the load is finished, the result is
  // reported through the delegate.
  void Load();

  // Stores credentials asynchronously to disk.
  void Store(const std::string& token, const std::string& device_id);

  // CloudPolicyData::Observer implementation:
  virtual void OnDeviceTokenChanged() OVERRIDE;
  virtual void OnCredentialsChanged() OVERRIDE;
  virtual void OnDataStoreGoingAway() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<UserPolicyTokenCache>;
  virtual ~UserPolicyTokenCache();

  void LoadOnFileThread();
  void NotifyOnUIThread(const std::string& token,
                        const std::string& device_id);
  void StoreOnFileThread(const std::string& token,
                         const std::string& device_id);

  const base::WeakPtr<CloudPolicyDataStore> data_store_;
  const FilePath cache_file_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyTokenCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_TOKEN_CACHE_H_
