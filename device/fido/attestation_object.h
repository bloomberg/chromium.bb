// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ATTESTATION_OBJECT_H_
#define DEVICE_FIDO_ATTESTATION_OBJECT_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "device/fido/authenticator_data.h"

namespace device {

class AttestationStatement;

// Object containing the authenticator-provided attestation every time
// a credential is created, per
// https://www.w3.org/TR/2017/WD-webauthn-20170505/#cred-attestation.
class AttestationObject {
 public:
  AttestationObject(AuthenticatorData data,
                    std::unique_ptr<AttestationStatement> statement);

  // Moveable.
  AttestationObject(AttestationObject&& other);
  AttestationObject& operator=(AttestationObject&& other);

  ~AttestationObject();

  // Replaces the attestation statement with a “none” attestation, as
  // specified for step 20.3 in
  // https://w3c.github.io/webauthn/#createCredential. (This does not,
  // currently, erase the AAGUID (in AttestedCredentialData in
  // |authenticator_data_|) because it is already always zero for U2F devices.
  // If CTAP2 is supported in the future, that will need to be taken into
  // account.)
  //
  // TODO(https://crbug.com/780078): erase AAGUID when CTAP2 is supported.
  void EraseAttestationStatement();

  // Produces a CBOR-encoded byte-array in the following format:
  // {"authData": authenticator data bytes,
  //  "fmt": attestation format name,
  //  "attStmt": attestation statement bytes }
  std::vector<uint8_t> SerializeToCBOREncodedBytes() const;

 private:
  AuthenticatorData authenticator_data_;
  std::unique_ptr<AttestationStatement> attestation_statement_;

  DISALLOW_COPY_AND_ASSIGN(AttestationObject);
};

}  // namespace device

#endif  // DEVICE_FIDO_ATTESTATION_OBJECT_H_
