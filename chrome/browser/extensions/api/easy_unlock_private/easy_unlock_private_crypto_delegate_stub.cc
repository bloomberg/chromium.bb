// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_crypto_delegate.h"

namespace extensions {
namespace api {

namespace {

// Stub EasyUnlockPrivateCryptoDelegate implementation.
class EasyUnlockPrivateCryptoDelegateStub
    : public extensions::api::EasyUnlockPrivateCryptoDelegate {
 public:
  EasyUnlockPrivateCryptoDelegateStub() {}

  virtual ~EasyUnlockPrivateCryptoDelegateStub() {}

  virtual void GenerateEcP256KeyPair(const KeyPairCallback& callback) OVERRIDE {
    callback.Run("", "");
  }

  virtual void PerformECDHKeyAgreement(const std::string& private_key,
                                       const std::string& public_key,
                                       const DataCallback& callback) OVERRIDE {
    callback.Run("");
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
    callback.Run("");
  }

  virtual void UnwrapSecureMessage(
      const std::string& message,
      const std::string& key,
      const std::string& associated_data,
      easy_unlock_private::EncryptionType encryption_type,
      easy_unlock_private::SignatureType signature_type,
      const DataCallback& callback) OVERRIDE {
    callback.Run("");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCryptoDelegateStub);
};

}  // namespace

// static
scoped_ptr<EasyUnlockPrivateCryptoDelegate>
    EasyUnlockPrivateCryptoDelegate::Create() {
  return scoped_ptr<EasyUnlockPrivateCryptoDelegate>(
      new EasyUnlockPrivateCryptoDelegateStub());
}

}  // namespace api
}  // namespace extensions
