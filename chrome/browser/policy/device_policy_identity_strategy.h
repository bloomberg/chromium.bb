// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
#define CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class TokenService;

namespace policy {

// DM token provider that stores the token in CrOS signed settings.
class DevicePolicyIdentityStrategy : public CloudPolicyIdentityStrategy,
                                     public NotificationObserver {
 public:
  DevicePolicyIdentityStrategy();
  virtual ~DevicePolicyIdentityStrategy();

  // Called by DevicePolicyIdentityStrategy::OwnershipChecker:
  virtual void OnOwnershipInformationAvailable(bool current_user_is_owner);

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
  class OwnershipChecker;

  // Recheck whether all parameters are available and if so, trigger a
  // credentials changed notification.
  void CheckAndTriggerFetch();

  // Updates the ownership information and then passes control to
  // |CheckAndTriggerFetch|.
  void CheckOwnershipAndTriggerFetch();

  // NotificationObserver method overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The machine identifier.
  std::string machine_id_;

  // The device identifier to be sent with requests. (This is actually more like
  // a session identifier since it is re-generated for each registration
  // request.)
  std::string device_id_;

  // Current token. Empty if not available.
  std::string device_token_;

  // Whether the currently logged in user is the device's owner. This variable
  // is owned by the UI thread but updated from the FILE thread. Therefore
  // after an owner login it will take some time before it turns to true.
  bool current_user_is_owner_;

  // Registers the provider for notification of successful Gaia logins.
  NotificationRegistrar registrar_;

  scoped_refptr<OwnershipChecker> ownership_checker_;

  // Allows to construct weak ptrs.
  base::WeakPtrFactory<DevicePolicyIdentityStrategy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyIdentityStrategy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
