// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_FIDO_ATTESTATION_STATEMENT_H_
#define CONTENT_BROWSER_WEBAUTH_FIDO_ATTESTATION_STATEMENT_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/cbor/cbor_values.h"
#include "content/browser/webauth/attestation_statement.h"
#include "content/common/content_export.h"

namespace content {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#fido-u2f-attestation
class CONTENT_EXPORT FidoAttestationStatement : public AttestationStatement {
 public:
  FidoAttestationStatement(std::vector<uint8_t> signature,
                           std::vector<std::vector<uint8_t>> x509_certificates);
  ~FidoAttestationStatement() override;

  static std::unique_ptr<FidoAttestationStatement>
  CreateFromU2fRegisterResponse(const std::vector<uint8_t>& u2f_data);

  // AttestationStatement overrides

  // Produces a map in the following format:
  // { "x5c": [ x509_certs bytes ], "sig": signature bytes ] }
  cbor::CBORValue::MapValue GetAsCBORMap() override;

 private:
  const std::vector<uint8_t> signature_;
  const std::vector<std::vector<uint8_t>> x509_certificates_;

  DISALLOW_COPY_AND_ASSIGN(FidoAttestationStatement);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_FIDO_ATTESTATION_STATEMENT_H_
