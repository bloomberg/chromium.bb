// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"

#include "chrome/common/extensions/api/easy_unlock_private.h"

namespace extensions {
namespace api {

EasyUnlockPrivatePerformECDHKeyAgreementFunction::
EasyUnlockPrivatePerformECDHKeyAgreementFunction() {}

EasyUnlockPrivatePerformECDHKeyAgreementFunction::
~EasyUnlockPrivatePerformECDHKeyAgreementFunction() {}

bool EasyUnlockPrivatePerformECDHKeyAgreementFunction::RunAsync() {
  return false;
}

void EasyUnlockPrivatePerformECDHKeyAgreementFunction::OnData(
    const std::string& secret_key) {
  if (!secret_key.empty()) {
    results_ = easy_unlock_private::PerformECDHKeyAgreement::Results::Create(
        secret_key);
  }
  SendResponse(true);
}

EasyUnlockPrivateGenerateEcP256KeyPairFunction::
EasyUnlockPrivateGenerateEcP256KeyPairFunction() {}

EasyUnlockPrivateGenerateEcP256KeyPairFunction::
~EasyUnlockPrivateGenerateEcP256KeyPairFunction() {}

bool EasyUnlockPrivateGenerateEcP256KeyPairFunction::RunAsync() {
  return false;
}

void EasyUnlockPrivateGenerateEcP256KeyPairFunction::OnData(
    const std::string& public_key,
    const std::string& private_key) {
  if (!public_key.empty() && !private_key.empty()) {
    results_ = easy_unlock_private::GenerateEcP256KeyPair::Results::Create(
        public_key, private_key);
  }
  SendResponse(true);
}

EasyUnlockPrivateCreateSecureMessageFunction::
EasyUnlockPrivateCreateSecureMessageFunction() {}

EasyUnlockPrivateCreateSecureMessageFunction::
~EasyUnlockPrivateCreateSecureMessageFunction() {}

bool EasyUnlockPrivateCreateSecureMessageFunction::RunAsync() {
  return false;
}

void EasyUnlockPrivateCreateSecureMessageFunction::OnData(
    const std::string& message) {
  if (!message.empty()) {
    results_ = easy_unlock_private::CreateSecureMessage::Results::Create(
        message);
  }
  SendResponse(true);
}

EasyUnlockPrivateUnwrapSecureMessageFunction::
EasyUnlockPrivateUnwrapSecureMessageFunction() {}

EasyUnlockPrivateUnwrapSecureMessageFunction::
~EasyUnlockPrivateUnwrapSecureMessageFunction() {}

bool EasyUnlockPrivateUnwrapSecureMessageFunction::RunAsync() {
  return false;
}

void EasyUnlockPrivateUnwrapSecureMessageFunction::OnData(
    const std::string& data) {
  if (!data.empty())
    results_ = easy_unlock_private::UnwrapSecureMessage::Results::Create(data);
  SendResponse(true);
}

}  // namespace api
}  // namespace extensions
