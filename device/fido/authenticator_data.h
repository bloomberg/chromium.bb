// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_DATA_H_
#define DEVICE_FIDO_AUTHENTICATOR_DATA_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/attested_credential_data.h"

namespace device {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#sec-authenticator-data.
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorData {
 public:
  enum class Flag : uint8_t {
    kTestOfUserPresence = 1u << 0,
    kAttestation = 1u << 6
  };

  AuthenticatorData(std::vector<uint8_t> application_parameter,
                    uint8_t flags,
                    std::vector<uint8_t> counter,
                    base::Optional<AttestedCredentialData> data);

  // Moveable.
  AuthenticatorData(AuthenticatorData&& other);
  AuthenticatorData& operator=(AuthenticatorData&& other);

  ~AuthenticatorData();

  // Produces a byte array consisting of:
  // * hash(relying_party_id / appid)
  // * flags
  // * counter
  // * attestation_data.
  std::vector<uint8_t> SerializeToByteArray() const;

 private:
  // The application parameter: a SHA-256 hash of either the RP ID or the AppID
  // associated with the credential.
  std::vector<uint8_t> application_parameter_;

  // Flags (bit 0 is the least significant bit):
  // [ED | AT | RFU | RFU | RFU | RFU | RFU | UP ]
  //  * Bit 0: Test of User Presence (TUP) result.
  //  * Bits 1-5: Reserved for future use (RFU).
  //  * Bit 6: Attestation data included (AT).
  //  * Bit 7: Extension data included (ED).
  uint8_t flags_;

  // Signature counter, 32-bit unsigned big-endian integer.
  std::vector<uint8_t> counter_;
  base::Optional<AttestedCredentialData> attested_data_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorData);
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_DATA_H_
