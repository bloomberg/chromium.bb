// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_crypto_provider.h"

#include "base/logging.h"

namespace local_discovery {

namespace {

// A stub session type used for development/debugging.
const char kAuthMethodEmpty[] = "empty";

const char kHandshakeStateComplete[] = "complete";
const char kStubVerificationCode[] = "SAMPLE";
}

class PrivetV3CryptoProviderEmpty : public PrivetV3CryptoProvider {
 public:
  PrivetV3CryptoProviderEmpty();
  virtual ~PrivetV3CryptoProviderEmpty();

  // PrivetV3CryptoProvider implementation.
  virtual HandshakeState GetState() OVERRIDE;
  virtual std::string GetAuthMethod() OVERRIDE;
  virtual HandshakeState GetNextStep(int* step, std::string* package) OVERRIDE;
  virtual HandshakeState SetStepResponse(int step,
                                         const std::string& state,
                                         const std::string& package) OVERRIDE;
  virtual std::string GetVerificationCode() OVERRIDE;
  virtual HandshakeState AcceptVerificationCode() OVERRIDE;
  virtual bool EncryptData(const std::string& input,
                           std::string* output) OVERRIDE;

 private:
  HandshakeState state_;
};

scoped_ptr<PrivetV3CryptoProvider> Create(
    const std::vector<std::string>& available_auth_methods) {
  for (size_t i = 0; i < available_auth_methods.size(); i++) {
    if (available_auth_methods[i] == kAuthMethodEmpty) {
      return scoped_ptr<PrivetV3CryptoProvider>(
          new PrivetV3CryptoProviderEmpty());
    }
  }

  return scoped_ptr<PrivetV3CryptoProvider>();
}

PrivetV3CryptoProviderEmpty::PrivetV3CryptoProviderEmpty()
    : state_(IN_PROGRESS) {
}

PrivetV3CryptoProviderEmpty::~PrivetV3CryptoProviderEmpty() {
}

PrivetV3CryptoProvider::HandshakeState PrivetV3CryptoProviderEmpty::GetState() {
  return state_;
}

std::string PrivetV3CryptoProviderEmpty::GetAuthMethod() {
  return kAuthMethodEmpty;
}

PrivetV3CryptoProvider::HandshakeState PrivetV3CryptoProviderEmpty::GetNextStep(
    int* step,
    std::string* package) {
  DCHECK(state_ == IN_PROGRESS);
  *step = 0;
  package->clear();
  state_ = AWAITING_RESPONSE;

  return state_;
}

PrivetV3CryptoProvider::HandshakeState
PrivetV3CryptoProviderEmpty::SetStepResponse(int step,
                                             const std::string& state,
                                             const std::string& package) {
  DCHECK(state_ == AWAITING_RESPONSE);

  bool success =
      (step == 0 && package.empty() && state == kHandshakeStateComplete);

  if (success) {
    state_ = AWAITING_USER_VERIFICATION;
  } else {
    state_ = HANDSHAKE_ERROR;
  }

  return state_;
}

std::string PrivetV3CryptoProviderEmpty::GetVerificationCode() {
  DCHECK(state_ == AWAITING_USER_VERIFICATION);
  return kStubVerificationCode;
}

PrivetV3CryptoProvider::HandshakeState
PrivetV3CryptoProviderEmpty::AcceptVerificationCode() {
  DCHECK(state_ == AWAITING_USER_VERIFICATION);
  return (state_ = HANDSHAKE_COMPLETE);
}

bool PrivetV3CryptoProviderEmpty::EncryptData(const std::string& input,
                                              std::string* output) {
  *output = input;
  return true;
}

}  // namespace local_discovery
