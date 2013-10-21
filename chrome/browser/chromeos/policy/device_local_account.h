// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_H_

#include <string>
#include <vector>

namespace chromeos {
class CrosSettings;
}

namespace policy {

// This must match DeviceLocalAccountInfoProto.AccountType in
// chrome_device_policy.proto.
struct DeviceLocalAccount {
  enum Type {
    // A login-less, policy-configured browsing session.
    TYPE_PUBLIC_SESSION,
    // An account that serves as a container for a single full-screen app.
    TYPE_KIOSK_APP,
    // Sentinel, must be last.
    TYPE_COUNT
  };

  DeviceLocalAccount(Type type,
                     const std::string& account_id,
                     const std::string& kiosk_app_id);
  ~DeviceLocalAccount();

  Type type;
  std::string account_id;
  std::string user_id;
  std::string kiosk_app_id;
};

std::string GenerateDeviceLocalAccountUserId(const std::string& account_id,
                                             DeviceLocalAccount::Type type);

// Determines whether |user_id| belongs to a device-local account and if so,
// returns the type of device-local account in |type| unless |type| is NULL.
bool IsDeviceLocalAccountUser(const std::string& user_id,
                              DeviceLocalAccount::Type* type);

// Stores a list of device-local accounts in |cros_settings|. The accounts are
// stored as a list of dictionaries with each dictionary containing the
// information about one |DeviceLocalAccount|.
void SetDeviceLocalAccounts(
    chromeos::CrosSettings* cros_settings,
    const std::vector<DeviceLocalAccount>& accounts);

// Retrieves a list of device-local accounts from |cros_settings|.
std::vector<DeviceLocalAccount> GetDeviceLocalAccounts(
    chromeos::CrosSettings* cros_settings);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_H_
