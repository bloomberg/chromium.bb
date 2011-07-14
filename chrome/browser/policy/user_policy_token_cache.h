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

// Handles disk access and threading details for loading and storing tokens.
class UserPolicyTokenLoader
    : public base::RefCountedThreadSafe<UserPolicyTokenLoader> {
 public:
  // Callback interface for reporting a successfull load.
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnTokenLoaded(const std::string& token,
                               const std::string& device_id) = 0;
  };

  UserPolicyTokenLoader(const base::WeakPtr<Delegate>& delegate,
                        const FilePath& cache_file);

  // Starts loading the disk cache. After the load is finished, the result is
  // reported through the delegate.
  void Load();

  // Stores credentials asynchronously to disk.
  void Store(const std::string& token, const std::string& device_id);

 private:
  friend class base::RefCountedThreadSafe<UserPolicyTokenLoader>;
  ~UserPolicyTokenLoader();

  void LoadOnFileThread();
  void NotifyOnUIThread(const std::string& token,
                        const std::string& device_id);
  void StoreOnFileThread(const std::string& token,
                         const std::string& device_id);

  const base::WeakPtr<Delegate> delegate_;
  const FilePath cache_file_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyTokenLoader);
};

// Responsible for managing the on-disk token cache.
class UserPolicyTokenCache : public CloudPolicyDataStore::Observer,
                             public UserPolicyTokenLoader::Delegate {
 public:
  UserPolicyTokenCache(CloudPolicyDataStore* data_store,
                       const FilePath& cache_file);
  virtual ~UserPolicyTokenCache();

  // Starts loading the disk cache and installs the token in the data store.
  void Load();

  // UserPolicyTokenLoader::Delegate implementation:
  virtual void OnTokenLoaded(const std::string& token,
                             const std::string& device_id) OVERRIDE;

  // CloudPolicyDataStore::Observer implementation:
  virtual void OnDeviceTokenChanged() OVERRIDE;
  virtual void OnCredentialsChanged() OVERRIDE;

 private:
  base::WeakPtrFactory<UserPolicyTokenLoader::Delegate> weak_ptr_factory_;

  CloudPolicyDataStore* data_store_;
  scoped_refptr<UserPolicyTokenLoader> loader_;

  // The current token in the cache. Upon new token notifications, we compare
  // against this in order to avoid writing the file when there's no change.
  std::string token_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyTokenCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_TOKEN_CACHE_H_
