// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_ATTESTED_CREDENTIAL_DATA_H_
#define CONTENT_BROWSER_WEBAUTH_ATTESTED_CREDENTIAL_DATA_H_

#include "content/browser/webauth/public_key.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#sec-attestation-data
class CONTENT_EXPORT AttestedCredentialData {
 public:
  AttestedCredentialData(std::vector<uint8_t> aaguid,
                         std::vector<uint8_t> length,
                         std::vector<uint8_t> credential_id,
                         std::unique_ptr<PublicKey> public_key);
  virtual ~AttestedCredentialData();

  static std::unique_ptr<AttestedCredentialData> CreateFromU2fRegisterResponse(
      const std::vector<uint8_t>& u2f_data,
      std::vector<uint8_t> aaguid,
      std::unique_ptr<PublicKey> public_key);

  const std::vector<uint8_t>& credential_id() { return credential_id_; }

  // Produces a byte array consisting of:
  // * AAGUID (16 bytes)
  // * Len (2 bytes)
  // * Credential Id (Len bytes)
  // * Credential Public Key.
  std::vector<uint8_t> SerializeAsBytes();

 private:
  // The 16-byte AAGUID of the authenticator.
  const std::vector<uint8_t> aaguid_;

  // Big-endian length of the credential (i.e. key handle).
  const std::vector<uint8_t> credential_id_length_;
  const std::vector<uint8_t> credential_id_;
  const std::unique_ptr<PublicKey> public_key_;

  DISALLOW_COPY_AND_ASSIGN(AttestedCredentialData);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_ATTESTED_CREDENTIAL_DATA_H_
