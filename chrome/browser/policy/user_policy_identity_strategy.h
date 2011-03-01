// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_IDENTITY_STRATEGY_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_IDENTITY_STRATEGY_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Profile;

namespace policy {

class DeviceManagementBackend;

// A token provider implementation that provides a user device token for the
// user corresponding to a given profile.
class UserPolicyIdentityStrategy : public CloudPolicyIdentityStrategy,
                                   public NotificationObserver {
 public:
  UserPolicyIdentityStrategy(Profile* profile,
                             const FilePath& token_cache_file);
  virtual ~UserPolicyIdentityStrategy();

  // CloudPolicyIdentityStrategy implementation:
  virtual std::string GetDeviceToken();
  virtual std::string GetDeviceID();
  virtual std::string GetMachineID();
  virtual em::DeviceRegisterRequest_Type GetPolicyRegisterType();
  virtual std::string GetPolicyType();

  virtual bool GetCredentials(std::string* username,
                              std::string* auth_token);
  virtual void OnDeviceTokenAvailable(const std::string& token);

 private:
  class TokenCache;

  // Checks whether a new token should be fetched and if so, sends out a
  // notification.
  void CheckAndTriggerFetch();

  // Gets the current user.
  std::string GetCurrentUser();

  // Called from the token cache when the token has been loaded.
  void OnCacheLoaded(const std::string& token, const std::string& device_id);

  // NotificationObserver method overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The profile this provider is associated with.
  Profile* profile_;

  // Keeps the on-disk copy of the token.
  scoped_refptr<TokenCache> cache_;

  // The device ID we use.
  std::string device_id_;

  // Current device token. Empty if not available.
  std::string device_token_;

  // Registers the provider for notification of successful Gaia logins.
  NotificationRegistrar registrar_;

  // Allows to construct weak ptrs.
  base::WeakPtrFactory<UserPolicyIdentityStrategy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyIdentityStrategy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_IDENTITY_STRATEGY_H_
