// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_REGISTER_RESPONSE_DATA_H_
#define DEVICE_FIDO_REGISTER_RESPONSE_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/response_data.h"

namespace device {

class AttestationObject;

// See figure 2: https://goo.gl/rsgvXk
class COMPONENT_EXPORT(DEVICE_FIDO) RegisterResponseData : public ResponseData {
 public:
  static base::Optional<RegisterResponseData> CreateFromU2fRegisterResponse(
      const std::vector<uint8_t>& relying_party_id_hash,
      base::span<const uint8_t> u2f_data);

  RegisterResponseData();

  RegisterResponseData(std::vector<uint8_t> credential_id,
                       std::unique_ptr<AttestationObject> object);

  // Moveable.
  RegisterResponseData(RegisterResponseData&& other);
  RegisterResponseData& operator=(RegisterResponseData&& other);

  ~RegisterResponseData();

  // Replaces the attestation statement with a “none” attestation, as specified
  // for step 20.3 in https://w3c.github.io/webauthn/#createCredential.  (This
  // does not, currently, erase the AAGUID because it is already always zero
  // for U2F devices. If CTAP2 is supported in the future, that will need to be
  // taken into account.)
  //
  // TODO(https://crbug.com/780078): erase AAGUID when CTAP2 is supported.
  void EraseAttestationStatement();

  // Returns true if the attestation certificate is known to be inappropriately
  // identifying. Some tokens return unique attestation certificates even when
  // the bit to request that is not set. (Normal attestation certificates are
  // not indended to be trackable.)
  bool IsAttestationCertificateInappropriatelyIdentifying();

  std::vector<uint8_t> GetCBOREncodedAttestationObject() const;

 private:
  std::unique_ptr<AttestationObject> attestation_object_;

  DISALLOW_COPY_AND_ASSIGN(RegisterResponseData);
};

}  // namespace device

#endif  // DEVICE_FIDO_REGISTER_RESPONSE_DATA_H_
