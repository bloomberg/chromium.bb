// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
#define DEVICE_CTAP_PUBLIC_KEY_CREDENTIAL_PARAMS_H_

#include <string>
#include <tuple>
#include <vector>

#include "components/cbor/cbor_values.h"

namespace device {

// Data structure containing public key credential type(string) and
// cryptographic algorithm(integer) as specified by the CTAP spec. Used as a
// request parameter for AuthenticatorMakeCredential.
class PublicKeyCredentialParams {
 public:
  explicit PublicKeyCredentialParams(
      std::vector<std::tuple<std::string, int>> credential_params);
  PublicKeyCredentialParams(PublicKeyCredentialParams&& other);
  PublicKeyCredentialParams& operator=(PublicKeyCredentialParams&& other);
  ~PublicKeyCredentialParams();

  cbor::CBORValue ConvertToCBOR() const;

 private:
  std::vector<std::tuple<std::string, int>> public_key_credential_params_;

  DISALLOW_COPY_AND_ASSIGN(PublicKeyCredentialParams);
};

}  // namespace device

#endif  // DEVICE_CTAP_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
