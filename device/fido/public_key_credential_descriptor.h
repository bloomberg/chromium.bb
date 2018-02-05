// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/optional.h"
#include "components/cbor/cbor_values.h"

namespace device {

// Data structure containing public key credential type (string) and credential
// id (byte array) as specified in the CTAP spec. Used for exclude_list for
// AuthenticatorMakeCredential command and allow_list parameter for
// AuthenticatorGetAssertion command.
class PublicKeyCredentialDescriptor {
 public:
  static base::Optional<PublicKeyCredentialDescriptor> CreateFromCBORValue(
      const cbor::CBORValue& cbor);

  PublicKeyCredentialDescriptor(std::string credential_type,
                                std::vector<uint8_t> id);
  PublicKeyCredentialDescriptor(PublicKeyCredentialDescriptor&& other);
  PublicKeyCredentialDescriptor(const PublicKeyCredentialDescriptor& other);
  PublicKeyCredentialDescriptor& operator=(
      PublicKeyCredentialDescriptor&& other);
  PublicKeyCredentialDescriptor& operator=(
      const PublicKeyCredentialDescriptor& other);
  ~PublicKeyCredentialDescriptor();

  cbor::CBORValue ConvertToCBOR() const;

  const std::string& credential_type() const { return credential_type_; }
  const std::vector<uint8_t>& id() const { return id_; }

 private:
  std::string credential_type_;
  std::vector<uint8_t> id_;
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_
