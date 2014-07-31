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

  virtual void PerformECDHKeyAgreement(const std::string& private_key,
                                       const std::string& public_key,
                                       const DataCallback& callback) OVERRIDE {
    dbus_client_->PerformECDHKeyAgreement(private_key, public_key, callback);
  }

  virtual void CreateSecureMessage(
      const std::string& payload,
      const std::string& key,
      const std::string& associated_data,
      const std::string& public_metadata,
      const std::string& verification_key_id,
      easy_unlock_private::EncryptionType encryption_type,
      easy_unlock_private::SignatureType signature_type,
      const DataCallback& callback) OVERRIDE {
    dbus_client_->CreateSecureMessage(payload,
                                      key,
                                      associated_data,
                                      public_metadata,
                                      verification_key_id,
                                      EncryptionTypeToString(encryption_type),
                                      SignatureTypeToString(signature_type),
                                      callback);
  }

  virtual void UnwrapSecureMessage(
      const std::string& message,
      const std::string& key,
      const std::string& associated_data,
      easy_unlock_private::EncryptionType encryption_type,
      easy_unlock_private::SignatureType signature_type,
      const DataCallback& callback) OVERRIDE {
    dbus_client_->UnwrapSecureMessage(message,
                                      key,
                                      associated_data,
                                      EncryptionTypeToString(encryption_type),
                                      SignatureTypeToString(signature_type),
                                      callback);
  }

 private:
  scoped_ptr<chromeos::EasyUnlockClient> dbus_client_;

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
