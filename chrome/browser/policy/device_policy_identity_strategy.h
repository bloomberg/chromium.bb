// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
#define CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
#pragma once

#include <string>

#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class TokenService;

namespace policy {

// DM token provider that stores the token in CrOS signed settings.
class DevicePolicyIdentityStrategy : public CloudPolicyIdentityStrategy,
                                     public NotificationObserver {
 public:
  DevicePolicyIdentityStrategy();
  virtual ~DevicePolicyIdentityStrategy() {}

  // CloudPolicyIdentityStrategy implementation:
  virtual std::string GetDeviceToken();
  virtual std::string GetDeviceID();
  virtual bool GetCredentials(std::string* username,
                              std::string* auth_token);
  virtual void OnDeviceTokenAvailable(const std::string& token);

 private:
  // Recheck whether all parameters are available and if so, trigger a
  // credentials changed notification.
  void CheckAndTriggerFetch();

  // NotificationObserver method overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // The machine identifier.
  std::string machine_id_;

  // Current token. Empty if not available.
  std::string device_token_;

  // Whether to try and register. Device policy enrollment does not happen
  // automatically except for the case that the device gets claimed. This
  // situation is detected by listening for the OWNERSHIP_TAKEN notification.
  bool should_register_;

  // Registers the provider for notification of successful Gaia logins.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyIdentityStrategy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
