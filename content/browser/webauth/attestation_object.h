// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_ATTESTATION_OBJECT_H_
#define CONTENT_BROWSER_WEBAUTH_ATTESTATION_OBJECT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/browser/webauth/authenticator_data.h"
#include "content/common/content_export.h"

namespace content {

class AttestationStatement;

// Object containing the authenticator-provided attestation every time
// a credential is created, per
// https://www.w3.org/TR/2017/WD-webauthn-20170505/#cred-attestation.
class CONTENT_EXPORT AttestationObject {
 public:
  AttestationObject(std::unique_ptr<AuthenticatorData> data,
                    std::unique_ptr<AttestationStatement> statement);
  virtual ~AttestationObject();

  // Produces a CBOR-encoded byte-array in the following format:
  // {"authData": authenticator data bytes,
  //  "fmt": attestation format name,
  //  "attStmt": attestation statement bytes }
  std::vector<uint8_t> SerializeToCBOREncodedBytes();

 private:
  const std::unique_ptr<AuthenticatorData> authenticator_data_;
  const std::unique_ptr<AttestationStatement> attestation_statement_;

  DISALLOW_COPY_AND_ASSIGN(AttestationObject);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_ATTESTATION_OBJECT_H_
