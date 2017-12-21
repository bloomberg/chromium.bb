// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_ATTESTATION_STATEMENT_H_
#define CONTENT_BROWSER_WEBAUTH_ATTESTATION_STATEMENT_H_

#include "base/macros.h"
#include "components/cbor/cbor_values.h"

namespace content {

// A signed data object containing statements about a credential itself and
// the authenticator that created it.
// Each attestation statement format is defined by the following attributes:
// - The attestation statement format identifier.
// - The set of attestation types supported by the format.
// - The syntax of an attestation statement produced in this format.
// https://www.w3.org/TR/2017/WD-webauthn-20170505/#cred-attestation.
class AttestationStatement {
 public:
  virtual ~AttestationStatement();

  // The CBOR map data to be added to the attestation object, structured
  // in a way that is specified by its particular attestation format:
  // https://www.w3.org/TR/2017/WD-webauthn-20170505/#defined-attestation-formats
  // This is not a CBOR-encoded byte array, but the map that will be
  // nested within another CBOR object and encoded then.
  virtual cbor::CBORValue::MapValue GetAsCBORMap() = 0;

  const std::string& format_name() { return format_; }

 protected:
  AttestationStatement(std::string format);

 private:
  const std::string format_;

  DISALLOW_COPY_AND_ASSIGN(AttestationStatement);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_ATTESTATION_STATEMENT_H_
