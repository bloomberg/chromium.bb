// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/signature_algorithm.h"

#include "net/der/input.h"
#include "net/der/parser.h"

namespace net {

namespace {

// From RFC 5912:
//
//     sha1WithRSAEncryption OBJECT IDENTIFIER ::= {
//      iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1)
//      pkcs-1(1) 5 }
//
// In dotted notation: 1.2.840.113549.1.1.5
const uint8_t kOidSha1WithRsaEncryption[] =
    {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05};

// sha1WithRSASignature is a deprecated equivalent of
// sha1WithRSAEncryption.
//
// It originates from the NIST Open Systems Environment (OSE)
// Implementor's Workshop (OIW).
//
// It is supported for compatibility with Microsoft's certificate APIs and
// tools, particularly makecert.exe, which default(ed/s) to this OID for SHA-1.
//
// See also: https://bugzilla.mozilla.org/show_bug.cgi?id=1042479
//
// In dotted notation: 1.3.14.3.2.29
const uint8_t kOidSha1WithRsaSignature[] = {0x2b, 0x0e, 0x03, 0x02, 0x1d};

// From RFC 5912:
//
//     pkcs-1  OBJECT IDENTIFIER  ::=
//         { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 1 }

// From RFC 5912:
//
//     sha256WithRSAEncryption  OBJECT IDENTIFIER  ::=  { pkcs-1 11 }
//
// In dotted notation: 1.2.840.113549.1.1.11
const uint8_t kOidSha256WithRsaEncryption[] =
    {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};

// From RFC 5912:
//
//     sha384WithRSAEncryption  OBJECT IDENTIFIER  ::=  { pkcs-1 12 }
//
// In dotted notation: 1.2.840.113549.1.1.11
const uint8_t kOidSha384WithRsaEncryption[] =
    {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c};

// From RFC 5912:
//
//     sha512WithRSAEncryption  OBJECT IDENTIFIER  ::=  { pkcs-1 13 }
//
// In dotted notation: 1.2.840.113549.1.1.13
const uint8_t kOidSha512WithRsaEncryption[] =
    {0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d};

// From RFC 5912:
//
//     ecdsa-with-SHA1 OBJECT IDENTIFIER ::= {
//      iso(1) member-body(2) us(840) ansi-X9-62(10045)
//      signatures(4) 1 }
//
// In dotted notation: 1.2.840.10045.4.1
const uint8_t kOidEcdsaWithSha1[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01};

// From RFC 5912:
//
//     ecdsa-with-SHA256 OBJECT IDENTIFIER ::= {
//      iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
//      ecdsa-with-SHA2(3) 2 }
//
// In dotted notation: 1.2.840.10045.4.3.2
const uint8_t kOidEcdsaWithSha256[] =
    {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02};

// From RFC 5912:
//
//     ecdsa-with-SHA384 OBJECT IDENTIFIER ::= {
//      iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
//      ecdsa-with-SHA2(3) 3 }
//
// In dotted notation: 1.2.840.10045.4.3.3
const uint8_t kOidEcdsaWithSha384[] =
    {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03};

// From RFC 5912:
//
//     ecdsa-with-SHA512 OBJECT IDENTIFIER ::= {
//      iso(1) member-body(2) us(840) ansi-X9-62(10045) signatures(4)
//      ecdsa-with-SHA2(3) 4 }
//
// In dotted notation: 1.2.840.10045.4.3.4
const uint8_t kOidEcdsaWithSha512[] =
    {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04};

// RFC 5280 section 4.1.1.2 defines signatureAlgorithm as:
//
//     AlgorithmIdentifier  ::=  SEQUENCE  {
//          algorithm               OBJECT IDENTIFIER,
//          parameters              ANY DEFINED BY algorithm OPTIONAL  }
WARN_UNUSED_RESULT bool ParseAlgorithmIdentifier(const der::Input& input,
                                                 der::Input* algorithm,
                                                 der::Input* parameters) {
  der::Parser parser(input);

  der::Parser algorithm_identifier_parser;
  if (!parser.ReadSequence(&algorithm_identifier_parser))
    return false;

  // There shouldn't be anything after the sequence. This is by definition,
  // as the input to this function is expected to be a single
  // AlgorithmIdentifier.
  if (parser.HasMore())
    return false;

  if (!algorithm_identifier_parser.ReadTag(der::kOid, algorithm))
    return false;

  // Read the optional parameters to a der::Input. The parameters can be at
  // most one TLV (for instance NULL or a sequence).
  //
  // Note that nothing is allowed after the single optional "parameters" TLV.
  // This is because RFC 5912's notation for AlgorithmIdentifier doesn't
  // explicitly list an extension point after "parameters".
  *parameters = der::Input();
  if (algorithm_identifier_parser.HasMore() &&
      !algorithm_identifier_parser.ReadRawTLV(parameters)) {
    return false;
  }
  return !algorithm_identifier_parser.HasMore();
}

// Returns true if |input| is empty.
WARN_UNUSED_RESULT bool IsEmpty(const der::Input& input) {
  return input.Length() == 0;
}

// Returns true if the entirety of the input is a NULL value.
WARN_UNUSED_RESULT bool IsNull(const der::Input& input) {
  der::Parser parser(input);
  der::Input null_value;
  if (!parser.ReadTag(der::kNull, &null_value))
    return false;

  // NULL values are TLV encoded; the value is expected to be empty.
  if (!IsEmpty(null_value))
    return false;

  // By definition of this function, the entire input must be a NULL.
  return !parser.HasMore();
}

// Parses an RSA PKCS#1 v1.5 signature algorithm given the DER-encoded
// "parameters" from the parsed AlgorithmIdentifier, and the hash algorithm
// that was implied by the AlgorithmIdentifier's OID.
//
// Returns a nullptr on failure.
//
// RFC 5912 requires that the parameters for RSA PKCS#1 v1.5 algorithms be NULL
// ("PARAMS TYPE NULL ARE required"):
//
//     sa-rsaWithSHA1 SIGNATURE-ALGORITHM ::= {
//      IDENTIFIER sha1WithRSAEncryption
//      PARAMS TYPE NULL ARE required
//      HASHES { mda-sha1 }
//      PUBLIC-KEYS { pk-rsa }
//      SMIME-CAPS {IDENTIFIED BY sha1WithRSAEncryption }
//     }
//
//     sa-sha256WithRSAEncryption SIGNATURE-ALGORITHM ::= {
//         IDENTIFIER sha256WithRSAEncryption
//         PARAMS TYPE NULL ARE required
//         HASHES { mda-sha256 }
//         PUBLIC-KEYS { pk-rsa }
//         SMIME-CAPS { IDENTIFIED BY sha256WithRSAEncryption }
//     }
//
//     sa-sha384WithRSAEncryption SIGNATURE-ALGORITHM ::= {
//         IDENTIFIER sha384WithRSAEncryption
//         PARAMS TYPE NULL ARE required
//         HASHES { mda-sha384 }
//         PUBLIC-KEYS { pk-rsa }
//         SMIME-CAPS { IDENTIFIED BY sha384WithRSAEncryption }
//     }
//
//     sa-sha512WithRSAEncryption SIGNATURE-ALGORITHM ::= {
//         IDENTIFIER sha512WithRSAEncryption
//         PARAMS TYPE NULL ARE required
//         HASHES { mda-sha512 }
//         PUBLIC-KEYS { pk-rsa }
//         SMIME-CAPS { IDENTIFIED BY sha512WithRSAEncryption }
//     }
scoped_ptr<SignatureAlgorithm> ParseRsaPkcs1(DigestAlgorithm digest,
                                             const der::Input& params) {
  if (!IsNull(params))
    return nullptr;

  return SignatureAlgorithm::CreateRsaPkcs1(digest);
}

// Parses an ECDSA signature algorithm given the DER-encoded "parameters" from
// the parsed AlgorithmIdentifier, and the hash algorithm that was implied by
// the AlgorithmIdentifier's OID.
//
// On failure returns a nullptr.
//
// RFC 5912 requires that the parameters for ECDSA algorithms be absent
// ("PARAMS TYPE NULL ARE absent"):
//
//     sa-ecdsaWithSHA1 SIGNATURE-ALGORITHM ::= {
//      IDENTIFIER ecdsa-with-SHA1
//      VALUE ECDSA-Sig-Value
//      PARAMS TYPE NULL ARE absent
//      HASHES { mda-sha1 }
//      PUBLIC-KEYS { pk-ec }
//      SMIME-CAPS {IDENTIFIED BY ecdsa-with-SHA1 }
//     }
//
//     sa-ecdsaWithSHA256 SIGNATURE-ALGORITHM ::= {
//      IDENTIFIER ecdsa-with-SHA256
//      VALUE ECDSA-Sig-Value
//      PARAMS TYPE NULL ARE absent
//      HASHES { mda-sha256 }
//      PUBLIC-KEYS { pk-ec }
//      SMIME-CAPS { IDENTIFIED BY ecdsa-with-SHA256 }
//     }
//
//     sa-ecdsaWithSHA384 SIGNATURE-ALGORITHM ::= {
//      IDENTIFIER ecdsa-with-SHA384
//      VALUE ECDSA-Sig-Value
//      PARAMS TYPE NULL ARE absent
//      HASHES { mda-sha384 }
//      PUBLIC-KEYS { pk-ec }
//      SMIME-CAPS { IDENTIFIED BY ecdsa-with-SHA384 }
//     }
//
//     sa-ecdsaWithSHA512 SIGNATURE-ALGORITHM ::= {
//      IDENTIFIER ecdsa-with-SHA512
//      VALUE ECDSA-Sig-Value
//      PARAMS TYPE NULL ARE absent
//      HASHES { mda-sha512 }
//      PUBLIC-KEYS { pk-ec }
//      SMIME-CAPS { IDENTIFIED BY ecdsa-with-SHA512 }
//     }
scoped_ptr<SignatureAlgorithm> ParseEcdsa(DigestAlgorithm digest,
                                          const der::Input& params) {
  if (!IsEmpty(params))
    return nullptr;

  return SignatureAlgorithm::CreateEcdsa(digest);
}

}  // namespace

RsaPssParameters::RsaPssParameters(DigestAlgorithm mgf1_hash,
                                   uint32_t salt_length)
    : mgf1_hash_(mgf1_hash), salt_length_(salt_length) {
}

bool RsaPssParameters::Equals(const RsaPssParameters* other) const {
  return mgf1_hash_ == other->mgf1_hash_ && salt_length_ == other->salt_length_;
}

SignatureAlgorithm::~SignatureAlgorithm() {
}

scoped_ptr<SignatureAlgorithm> SignatureAlgorithm::CreateFromDer(
    const der::Input& algorithm_identifier) {
  der::Input oid;
  der::Input params;
  if (!ParseAlgorithmIdentifier(algorithm_identifier, &oid, &params))
    return nullptr;

  // TODO(eroman): Each OID is tested for equality in order, which is not
  // particularly efficient.

  if (oid.Equals(der::Input(kOidSha1WithRsaEncryption)))
    return ParseRsaPkcs1(DigestAlgorithm::Sha1, params);

  if (oid.Equals(der::Input(kOidSha256WithRsaEncryption)))
    return ParseRsaPkcs1(DigestAlgorithm::Sha256, params);

  if (oid.Equals(der::Input(kOidSha384WithRsaEncryption)))
    return ParseRsaPkcs1(DigestAlgorithm::Sha384, params);

  if (oid.Equals(der::Input(kOidSha512WithRsaEncryption)))
    return ParseRsaPkcs1(DigestAlgorithm::Sha512, params);

  if (oid.Equals(der::Input(kOidEcdsaWithSha1)))
    return ParseEcdsa(DigestAlgorithm::Sha1, params);

  if (oid.Equals(der::Input(kOidEcdsaWithSha256)))
    return ParseEcdsa(DigestAlgorithm::Sha256, params);

  if (oid.Equals(der::Input(kOidEcdsaWithSha384)))
    return ParseEcdsa(DigestAlgorithm::Sha384, params);

  if (oid.Equals(der::Input(kOidEcdsaWithSha512)))
    return ParseEcdsa(DigestAlgorithm::Sha512, params);

  // TODO(eroman): Add parsing of RSASSA-PSS

  if (oid.Equals(der::Input(kOidSha1WithRsaSignature)))
    return ParseRsaPkcs1(DigestAlgorithm::Sha1, params);

  return nullptr;  // Unsupported OID.
}

scoped_ptr<SignatureAlgorithm> SignatureAlgorithm::CreateRsaPkcs1(
    DigestAlgorithm digest) {
  return make_scoped_ptr(
      new SignatureAlgorithm(SignatureAlgorithmId::RsaPkcs1, digest, nullptr));
}

scoped_ptr<SignatureAlgorithm> SignatureAlgorithm::CreateEcdsa(
    DigestAlgorithm digest) {
  return make_scoped_ptr(
      new SignatureAlgorithm(SignatureAlgorithmId::Ecdsa, digest, nullptr));
}

scoped_ptr<SignatureAlgorithm> SignatureAlgorithm::CreateRsaPss(
    DigestAlgorithm digest,
    DigestAlgorithm mgf1_hash,
    uint32_t salt_length) {
  return make_scoped_ptr(new SignatureAlgorithm(
      SignatureAlgorithmId::RsaPss, digest,
      make_scoped_ptr(new RsaPssParameters(mgf1_hash, salt_length))));
}

bool SignatureAlgorithm::Equals(const SignatureAlgorithm& other) const {
  if (algorithm_ != other.algorithm_)
    return false;

  if (digest_ != other.digest_)
    return false;

  // Check that the parameters are equal.
  switch (algorithm_) {
    case SignatureAlgorithmId::RsaPss: {
      const RsaPssParameters* params1 = ParamsForRsaPss();
      const RsaPssParameters* params2 = other.ParamsForRsaPss();
      if (!params1 || !params2 || !params1->Equals(params2))
        return false;
      break;
    }

    // There shouldn't be any parameters.
    case SignatureAlgorithmId::RsaPkcs1:
    case SignatureAlgorithmId::Ecdsa:
      if (params_ || other.params_)
        return false;
      break;
  }

  return true;
}

const RsaPssParameters* SignatureAlgorithm::ParamsForRsaPss() const {
  if (algorithm_ == SignatureAlgorithmId::RsaPss)
    return static_cast<RsaPssParameters*>(params_.get());
  return nullptr;
}

SignatureAlgorithm::SignatureAlgorithm(
    SignatureAlgorithmId algorithm,
    DigestAlgorithm digest,
    scoped_ptr<SignatureAlgorithmParameters> params)
    : algorithm_(algorithm), digest_(digest), params_(params.Pass()) {
}

}  // namespace net
