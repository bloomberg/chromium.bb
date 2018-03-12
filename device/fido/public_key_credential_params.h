// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "components/cbor/cbor_values.h"
#include "device/fido/ctap_constants.h"

namespace device {

// Data structure containing public key credential type(string) and
// cryptographic algorithm(integer) as specified by the CTAP spec. Used as a
// request parameter for AuthenticatorMakeCredential.
class PublicKeyCredentialParams {
 public:
  struct CredentialInfo {
    std::string type;
    int algorithm =
        base::strict_cast<int>(kCoseAlgorithmIdentifier::kCoseEs256);
  };

  explicit PublicKeyCredentialParams(
      std::vector<CredentialInfo> credential_params);
  PublicKeyCredentialParams(PublicKeyCredentialParams&& other);
  PublicKeyCredentialParams& operator=(PublicKeyCredentialParams&& other);
  ~PublicKeyCredentialParams();

  cbor::CBORValue ConvertToCBOR() const;
  const std::vector<CredentialInfo>& public_key_credential_params() const {
    return public_key_credential_params_;
  }

 private:
  std::vector<CredentialInfo> public_key_credential_params_;

  DISALLOW_COPY_AND_ASSIGN(PublicKeyCredentialParams);
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
