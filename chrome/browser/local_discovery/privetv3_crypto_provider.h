// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_CRYPTO_PROVIDER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_CRYPTO_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace local_discovery {

class PrivetV3CryptoProvider {
 public:
  enum HandshakeState {
    // Handshake still in progress. Call |GetNextStep| to send next handshake
    // step.
    IN_PROGRESS,
    // Handshake in progress, waiting for response. Call |SetStepResponse| to
    // set the step response.
    AWAITING_RESPONSE,
    // Handshake in progress, need to wait for user verification to
    // continue. Call |GetVerificationCode| to get the verification code and
    // |AcceptVerificationCode| to signify the code is accepted.
    AWAITING_USER_VERIFICATION,
    // Handshake complete. Call |EncryptData| to encrypt the data.
    HANDSHAKE_COMPLETE,
    // Handshake error.
    HANDSHAKE_ERROR
  };

  virtual ~PrivetV3CryptoProvider() {}

  static scoped_ptr<PrivetV3CryptoProvider> Create(
      const std::vector<std::string>& available_auth_methods);

  // Return the current state of the crypto provider.
  virtual HandshakeState GetState() = 0;

  // Return the authentication method used.
  virtual std::string GetAuthMethod() = 0;

  // Get the next handshake command. |step| is the step number to send,
  // |package| is a base64-encoded package to send with the step. Return
  // |true| if a package is generated or |false| in case of an error.
  virtual HandshakeState GetNextStep(int* step, std::string* package) = 0;

  // Input the response to the handshake command. |step| is the received step
  // number, |state| is the received state string, |package| is the received
  // base64-encoded package. Return the current handshake state.
  virtual HandshakeState SetStepResponse(int step,
                                         const std::string& state,
                                         const std::string& package) = 0;

  // Get the verification code to be displayed on the screen.
  virtual std::string GetVerificationCode() = 0;

  // Signal that the verification code is accepted. Returns the current
  // handshake state.
  virtual HandshakeState AcceptVerificationCode() = 0;

  // Encrypt a string using the session key.
  virtual bool EncryptData(const std::string& input, std::string* output) = 0;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_CRYPTO_PROVIDER_H_
