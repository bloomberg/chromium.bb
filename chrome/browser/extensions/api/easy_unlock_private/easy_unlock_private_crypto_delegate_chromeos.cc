// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_crypto_delegate.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {
namespace api {

namespace {

// Converts encryption type to a string representation used by EasyUnlock dbus
// client.
std::string EncryptionTypeToString(easy_unlock_private::EncryptionType type) {
  switch (type) {
    case easy_unlock_private::ENCRYPTION_TYPE_AES_256_CBC:
      return easy_unlock::kEncryptionTypeAES256CBC;
    default:
      return easy_unlock::kEncryptionTypeNone;
  }
}

// Converts signature type to a string representation used by EasyUnlock dbus
// client.
std::string SignatureTypeToString(easy_unlock_private::SignatureType type) {
  switch (type) {
    case easy_unlock_private::SIGNATURE_TYPE_ECDSA_P256_SHA256:
      return easy_unlock::kSignatureTypeECDSAP256SHA256;
    case easy_unlock_private::SIGNATURE_TYPE_HMAC_SHA256:
      // Fall through to default.
    default:
      return easy_unlock::kSignatureTypeHMACSHA256;
  }
}

// ChromeOS specific EasyUnlockPrivateCryptoDelegate implementation.
class EasyUnlockPrivateCryptoDelegateChromeOS
    : public extensions::api::EasyUnlockPrivateCryptoDelegate {
 public:
  EasyUnlockPrivateCryptoDelegateChromeOS()
      : dbus_client_(
            chromeos::DBusThreadManager::Get()->GetEasyUnlockClient()) {
  }

  virtual ~EasyUnlockPrivateCryptoDelegateChromeOS() {}

  virtual void GenerateEcP256KeyPair(const KeyPairCallback& callback) OVERRIDE {
    dbus_client_->GenerateEcP256KeyPair(callback);
  }

  virtual void PerformECDHKeyAgreement(
      const easy_unlock_private::PerformECDHKeyAgreement::Params& params,
      const DataCallback& callback) OVERRIDE {
    dbus_client_->PerformECDHKeyAgreement(
        params.private_key,
        params.public_key,
        callback);
  }

  virtual void CreateSecureMessage(
      const easy_unlock_private::CreateSecureMessage::Params& params,
      const DataCallback& callback) OVERRIDE {
    chromeos::EasyUnlockClient::CreateSecureMessageOptions options;
    options.key = params.key;
    if (params.options.associated_data)
      options.associated_data = *params.options.associated_data;
    if (params.options.public_metadata)
      options.public_metadata = *params.options.public_metadata;
    if (params.options.verification_key_id)
      options.verification_key_id = *params.options.verification_key_id;
    if (params.options.decryption_key_id)
      options.decryption_key_id = *params.options.decryption_key_id;
    options.encryption_type =
        EncryptionTypeToString(params.options.encrypt_type);
    options.signature_type =
        SignatureTypeToString(params.options.sign_type);

    dbus_client_->CreateSecureMessage(params.payload, options, callback);
  }

  virtual void UnwrapSecureMessage(
      const easy_unlock_private::UnwrapSecureMessage::Params& params,
      const DataCallback& callback) OVERRIDE {
    chromeos::EasyUnlockClient::UnwrapSecureMessageOptions options;
    options.key = params.key;
    if (params.options.associated_data)
      options.associated_data = *params.options.associated_data;
    options.encryption_type =
        EncryptionTypeToString(params.options.encrypt_type);
    options.signature_type =
        SignatureTypeToString(params.options.sign_type);

    dbus_client_->UnwrapSecureMessage(params.secure_message, options, callback);
  }

 private:
  chromeos::EasyUnlockClient* dbus_client_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCryptoDelegateChromeOS);
};

}  // namespace

// static
scoped_ptr<EasyUnlockPrivateCryptoDelegate>
    EasyUnlockPrivateCryptoDelegate::Create() {
  return scoped_ptr<EasyUnlockPrivateCryptoDelegate>(
      new EasyUnlockPrivateCryptoDelegateChromeOS());
}

}  // namespace api
}  // namespace extensions
