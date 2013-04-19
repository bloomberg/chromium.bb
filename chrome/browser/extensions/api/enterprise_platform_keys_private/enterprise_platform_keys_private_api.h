// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class PrefRegistrySyncable;
class PrefService;

namespace chromeos {
class CryptohomeClient;
}  // namespace chromeos

namespace cryptohome {
class AsyncMethodCaller;
}  // namespace cryptohome

namespace policy {
class EnterpriseInstallAttributes;
}  // namespace policy

namespace extensions {

class EPKPChallengeKeyBase : public AsyncExtensionFunction {
 protected:
  EPKPChallengeKeyBase();
  virtual ~EPKPChallengeKeyBase();

  // Returns a trusted value from CroSettings indicating if the device
  // attestation is enabled.
  void GetDeviceAttestationEnabled(
      const base::Callback<void(bool)>& callback) const;

  // Returns true if the device is enterprise managed.
  bool IsEnterpriseDevice() const;

  // Returns the enterprise domain the device is enrolled to.
  std::string GetEnterpriseDomain() const;

  // Returns the enterprise virtual device ID.
  std::string GetDeviceId() const;

  chromeos::CryptohomeClient* cryptohome_client_;
  cryptohome::AsyncMethodCaller* async_caller_;

 private:
  policy::EnterpriseInstallAttributes* install_attributes_;
};

class EPKPChallengeMachineKey : public EPKPChallengeKeyBase {
 protected:
  virtual bool RunImpl() OVERRIDE;

 private:
  static const char kKeyName[];

  virtual ~EPKPChallengeMachineKey();

  void GetDeviceAttestationEnabledCallback(const std::string& challenge,
                                           bool enabled);
  void SignChallengeCallback(bool success, const std::string& response);

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeMachineKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEMACHINEKEY);
};

typedef EPKPChallengeMachineKey
    EnterprisePlatformKeysPrivateChallengeMachineKeyFunction;

class EPKPChallengeUserKey : public EPKPChallengeKeyBase {
 public:
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

 protected:
  virtual bool RunImpl() OVERRIDE;

 private:
  static const char kKeyName[];

  virtual ~EPKPChallengeUserKey();

  void GetDeviceAttestationEnabledCallback(const std::string& challenge,
                                           bool register_key,
                                           const std::string& domain,
                                           bool enabled);
  void UserConsentCallback(const std::string& challenge,
                           bool register_key,
                           const std::string& domain,
                           bool action);
  void SignChallengeCallback(bool register_key,
                             bool success,
                             const std::string& response);
  void RegisterKeyCallback(const std::string& response,
                           bool success,
                           cryptohome::MountError return_code);

  void AskForUserConsent(const base::Callback<void(bool)>& callback);
  bool IsExtensionWhitelisted() const;
  bool IsRemoteAttestationEnabledForUser() const;
  std::string GetUserDomain() const;

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeUserKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEUSERKEY);
};

typedef EPKPChallengeUserKey
    EnterprisePlatformKeysPrivateChallengeUserKeyFunction;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
