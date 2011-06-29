// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_IDENTITY_STRATEGY_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_IDENTITY_STRATEGY_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/browser/policy/user_policy_token_cache.h"

namespace policy {

class DeviceManagementBackend;

// A token provider implementation that provides a user device token for the
// user corresponding to given credentials.
class UserPolicyIdentityStrategy : public CloudPolicyIdentityStrategy,
                                   public UserPolicyTokenCache::Delegate {
 public:
  UserPolicyIdentityStrategy(const std::string& user_name,
                             const FilePath& token_cache_file);
  virtual ~UserPolicyIdentityStrategy();

  // Start loading the token cache.
  void LoadTokenCache();

  // Set a newly arriving auth_token and maybe trigger a fetch.
  void SetAuthToken(const std::string& auth_token);

  // CloudPolicyIdentityStrategy implementation:
  virtual std::string GetDeviceToken() OVERRIDE;
  virtual std::string GetDeviceID() OVERRIDE;
  virtual std::string GetMachineID() OVERRIDE;
  virtual std::string GetMachineModel() OVERRIDE;
  virtual em::DeviceRegisterRequest_Type GetPolicyRegisterType() OVERRIDE;
  virtual std::string GetPolicyType() OVERRIDE;
  virtual bool GetCredentials(std::string* username,
                              std::string* auth_token) OVERRIDE;
  virtual void OnDeviceTokenAvailable(const std::string& token) OVERRIDE;

 private:
  // Checks whether a new token should be fetched and if so, sends out a
  // notification.
  void CheckAndTriggerFetch();

  // Gets the current user.
  std::string GetCurrentUser();

  // Called from the token cache when the token has been loaded.
  virtual void OnTokenCacheLoaded(const std::string& token,
                                  const std::string& device_id) OVERRIDE;

  // Keeps the on-disk copy of the token.
  scoped_refptr<UserPolicyTokenCache> cache_;

  // false until cache_ reports being loaded for the first time, true
  // afterwards.
  bool cache_loaded_;

  // The device ID we use.
  std::string device_id_;

  // Current device token. Empty if not available.
  std::string device_token_;

  // Current auth token. Empty if not available.
  std::string auth_token_;

  // Current user name. Empty if not available. This is set on creation and not
  // changed afterwards.
  std::string user_name_;

  // Allows to construct weak ptrs.
  base::WeakPtrFactory<UserPolicyTokenCache::Delegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyIdentityStrategy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_IDENTITY_STRATEGY_H_
