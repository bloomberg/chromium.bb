// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class PrefService;

namespace chromeos {
class CryptohomeClient;
}

namespace cryptohome {
class AsyncMethodCaller;
}

namespace policy {
class EnterpriseInstallAttributes;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {

class EPKPChallengeKeyBase : public ChromeAsyncExtensionFunction {
 public:
  static const char kChallengeBadBase64Error[];
  static const char kDevicePolicyDisabledError[];
  static const char kExtensionNotWhitelistedError[];
  static const char kResponseBadBase64Error[];
  static const char kSignChallengeFailedError[];
  static const char kUserNotManaged[];

 protected:
  enum PrepareKeyResult {
    PREPARE_KEY_OK = 0,
    PREPARE_KEY_DBUS_ERROR,
    PREPARE_KEY_USER_REJECTED,
    PREPARE_KEY_GET_CERTIFICATE_FAILED,
    PREPARE_KEY_RESET_REQUIRED
  };

  EPKPChallengeKeyBase();
  EPKPChallengeKeyBase(
      chromeos::CryptohomeClient* cryptohome_client,
      cryptohome::AsyncMethodCaller* async_caller,
      chromeos::attestation::AttestationFlow* attestation_flow,
      policy::EnterpriseInstallAttributes* install_attributes);
  virtual ~EPKPChallengeKeyBase();

  // Returns a trusted value from CroSettings indicating if the device
  // attestation is enabled.
  void GetDeviceAttestationEnabled(
      const base::Callback<void(bool)>& callback) const;

  // Returns true if the device is enterprise managed.
  bool IsEnterpriseDevice() const;

  // Returns true if the extension is white-listed in the user policy.
  bool IsExtensionWhitelisted() const;

  // Returns true if the user is enterprise managed.
  bool IsUserManaged() const;

  // Returns the enterprise domain the device is enrolled to.
  std::string GetEnterpriseDomain() const;

  // Returns the user email.
  std::string GetUserEmail() const;

  // Returns the enterprise virtual device ID.
  std::string GetDeviceId() const;

  // Prepares the key for signing. It will first check if the key exists. If
  // the key does not exist, it will call AttestationFlow::GetCertificate() to
  // get a new one. If require_user_consent is true, it will explicitly ask for
  // user consent before calling GetCertificate().
  void PrepareKey(
      chromeos::attestation::AttestationKeyType key_type,
      const std::string& user_id,
      const std::string& key_name,
      chromeos::attestation::AttestationCertificateProfile certificate_profile,
      bool require_user_consent,
      const base::Callback<void(PrepareKeyResult)>& callback);

  chromeos::CryptohomeClient* cryptohome_client_;
  cryptohome::AsyncMethodCaller* async_caller_;
  chromeos::attestation::AttestationFlow* attestation_flow_;
  scoped_ptr<chromeos::attestation::AttestationFlow> default_attestation_flow_;

 private:
  // Holds the context of a PrepareKey() operation.
  struct PrepareKeyContext {
    PrepareKeyContext(
        chromeos::attestation::AttestationKeyType key_type,
        const std::string& user_id,
        const std::string& key_name,
        chromeos::attestation::AttestationCertificateProfile
            certificate_profile,
        bool require_user_consent,
        const base::Callback<void(PrepareKeyResult)>& callback);
    ~PrepareKeyContext();

    chromeos::attestation::AttestationKeyType key_type;
    const std::string user_id;
    const std::string key_name;
    chromeos::attestation::AttestationCertificateProfile certificate_profile;
    bool require_user_consent;
    const base::Callback<void(PrepareKeyResult)> callback;
  };

  void IsAttestationPreparedCallback(
      const PrepareKeyContext& context,
      chromeos::DBusMethodCallStatus status,
      bool result);
  void DoesKeyExistCallback(
      const PrepareKeyContext& context,
      chromeos::DBusMethodCallStatus status,
      bool result);
  void AskForUserConsent(const base::Callback<void(bool)>& callback) const;
  void AskForUserConsentCallback(
      const PrepareKeyContext& context,
      bool result);
  void GetCertificateCallback(
      const base::Callback<void(PrepareKeyResult)>& callback,
      bool success,
      const std::string& pem_certificate_chain);

  policy::EnterpriseInstallAttributes* install_attributes_;
};

class EPKPChallengeMachineKey : public EPKPChallengeKeyBase {
 public:
  static const char kGetCertificateFailedError[];
  static const char kNonEnterpriseDeviceError[];

  EPKPChallengeMachineKey();
  EPKPChallengeMachineKey(
      chromeos::CryptohomeClient* cryptohome_client,
      cryptohome::AsyncMethodCaller* async_caller,
      chromeos::attestation::AttestationFlow* attestation_flow,
      policy::EnterpriseInstallAttributes* install_attributes);

 protected:
  virtual bool RunAsync() OVERRIDE;

 private:
  static const char kKeyName[];

  virtual ~EPKPChallengeMachineKey();

  void GetDeviceAttestationEnabledCallback(const std::string& challenge,
                                           bool enabled);
  void PrepareKeyCallback(const std::string& challenge,
                          PrepareKeyResult result);
  void SignChallengeCallback(bool success, const std::string& response);

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeMachineKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEMACHINEKEY);
};

typedef EPKPChallengeMachineKey
    EnterprisePlatformKeysPrivateChallengeMachineKeyFunction;

class EPKPChallengeUserKey : public EPKPChallengeKeyBase {
 public:
  static const char kGetCertificateFailedError[];
  static const char kKeyRegistrationFailedError[];
  static const char kUserPolicyDisabledError[];

  EPKPChallengeUserKey();
  EPKPChallengeUserKey(
      chromeos::CryptohomeClient* cryptohome_client,
      cryptohome::AsyncMethodCaller* async_caller,
      chromeos::attestation::AttestationFlow* attestation_flow,
      policy::EnterpriseInstallAttributes* install_attributes);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  virtual bool RunAsync() OVERRIDE;

 private:
  static const char kKeyName[];

  virtual ~EPKPChallengeUserKey();

  void GetDeviceAttestationEnabledCallback(const std::string& challenge,
                                           bool register_key,
                                           bool require_user_consent,
                                           bool enabled);
  void PrepareKeyCallback(const std::string& challenge,
                          bool register_key,
                          PrepareKeyResult result);
  void SignChallengeCallback(bool register_key,
                             bool success,
                             const std::string& response);
  void RegisterKeyCallback(const std::string& response,
                           bool success,
                           cryptohome::MountError return_code);

  bool IsRemoteAttestationEnabledForUser() const;

  DECLARE_EXTENSION_FUNCTION(
      "enterprise.platformKeysPrivate.challengeUserKey",
      ENTERPRISE_PLATFORMKEYSPRIVATE_CHALLENGEUSERKEY);
};

typedef EPKPChallengeUserKey
    EnterprisePlatformKeysPrivateChallengeUserKeyFunction;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_PRIVATE_ENTERPRISE_PLATFORM_KEYS_PRIVATE_API_H__
