// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
#define CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"

class TokenService;

namespace policy {

// DM token provider that stores the token in CrOS signed settings.
class DevicePolicyIdentityStrategy : public CloudPolicyIdentityStrategy {
 public:
  DevicePolicyIdentityStrategy();
  virtual ~DevicePolicyIdentityStrategy();

  // Sets (GAIA) auth credentials of the owner of the device during device
  // enrollment. This automatically triggers fetching a DMToken that can
  // be used for future authentication with DMServer.
  void SetAuthCredentials(const std::string& username,
                          const std::string& auth_token);

  // Sets the device's credentials when they have been read from disk after
  // a reboot.
  void SetDeviceManagementCredentials(const std::string& owner_email,
                                      const std::string& device_id,
                                      const std::string& device_token);

  // Initiates a policy fetch after a successful device registration. This
  // function should be called only after the device token has been fetched
  // either through the DMServer or loaded from the cache.
  void FetchPolicy();

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
  // The e-mail and auth token of the device owner. Set by |SetCredentials()|.
  std::string username_;
  std::string auth_token_;

  // The machine identifier and model.
  std::string machine_id_;
  std::string machine_model_;

  // The device identifier to be sent with requests. (This is actually more like
  // a session identifier since it is re-generated for each registration
  // request.)
  std::string device_id_;

  // Current token. Empty if not available.
  std::string device_token_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyIdentityStrategy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_POLICY_IDENTITY_STRATEGY_H_
