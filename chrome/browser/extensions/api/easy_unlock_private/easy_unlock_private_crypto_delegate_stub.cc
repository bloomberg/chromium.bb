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

  virtual void PerformECDHKeyAgreement(
      const easy_unlock_private::PerformECDHKeyAgreement::Params& params,
      const DataCallback& callback) OVERRIDE {
    callback.Run("");
  }

  virtual void CreateSecureMessage(
      const easy_unlock_private::CreateSecureMessage::Params& params,
      const DataCallback& callback) OVERRIDE {
    callback.Run("");
  }

  virtual void UnwrapSecureMessage(
      const easy_unlock_private::UnwrapSecureMessage::Params& params,
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
