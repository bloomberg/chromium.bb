// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cast/cast_cert_validator.h"

#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "extensions/browser/api/cast_channel/cast_auth_ica.h"
#include "net/cert/internal/certificate_policies.h"
#include "net/cert/internal/extended_key_usage.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/signature_policy.h"
#include "net/cert/internal/verify_certificate_chain.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_openssl.h"
#include "net/der/input.h"
#include "net/ssl/scoped_openssl_types.h"

namespace extensions {
namespace api {
namespace cast_crypto {
namespace {

// -------------------------------------------------------------------------
// Cast trust anchors.
// -------------------------------------------------------------------------

// There are two trusted roots for Cast certificate chains:
//
//   (1) CN=Cast Root CA
//   (2) CN=Eureka Root CA
//
// Note that only the subject/spki are saved here, not the full certificate.
// See the TODO in CreateCastTrustStore().

unsigned char kCastRootCaSubjectDer[119] = {
    0x30, 0x75, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08,
    0x0C, 0x0A, 0x43, 0x61, 0x6C, 0x69, 0x66, 0x6F, 0x72, 0x6E, 0x69, 0x61,
    0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0C, 0x0D, 0x4D,
    0x6F, 0x75, 0x6E, 0x74, 0x61, 0x69, 0x6E, 0x20, 0x56, 0x69, 0x65, 0x77,
    0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x0A, 0x47,
    0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20, 0x49, 0x6E, 0x63, 0x31, 0x0D, 0x30,
    0x0B, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x0C, 0x04, 0x43, 0x61, 0x73, 0x74,
    0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0C, 0x43,
    0x61, 0x73, 0x74, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41,
};

unsigned char kCastRootCaSpkiDer[294] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00,
    0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xBA, 0xD9, 0x65,
    0x9D, 0xDA, 0x39, 0xD3, 0xC1, 0x77, 0xF6, 0xD4, 0xD0, 0xAE, 0x8F, 0x58,
    0x08, 0x68, 0x39, 0x4A, 0x95, 0xED, 0x70, 0xCF, 0xFD, 0x79, 0x08, 0xA9,
    0xAA, 0xE5, 0xE9, 0xB8, 0xA7, 0x2D, 0xA0, 0x67, 0x47, 0x8A, 0x9E, 0xC9,
    0xCF, 0x70, 0xB3, 0x05, 0x87, 0x69, 0x11, 0xEC, 0x70, 0x98, 0x97, 0xC3,
    0xE6, 0xC3, 0xC3, 0xEB, 0xBD, 0xC6, 0xB0, 0x3D, 0xFC, 0x4F, 0xC1, 0x5E,
    0x38, 0x9F, 0xDA, 0xCF, 0x73, 0x30, 0x06, 0x5B, 0x79, 0x37, 0xC1, 0x5E,
    0x8C, 0x87, 0x47, 0x94, 0x9A, 0x41, 0x92, 0x2A, 0xD6, 0x95, 0xC4, 0x71,
    0x5C, 0x27, 0x5D, 0x08, 0xB1, 0x80, 0xC6, 0x92, 0xBD, 0x1B, 0xE3, 0x41,
    0x97, 0xA1, 0xEC, 0x75, 0x9F, 0x55, 0x9E, 0x3E, 0x9F, 0x8F, 0x1C, 0xC7,
    0x65, 0x64, 0x07, 0xD3, 0xB3, 0x96, 0xA1, 0x04, 0x9F, 0x91, 0xC4, 0xDE,
    0x0A, 0x7B, 0x6C, 0xD9, 0xC8, 0xC0, 0x78, 0x31, 0xA0, 0x19, 0x42, 0xA9,
    0xE8, 0x83, 0xE3, 0xCE, 0xFC, 0xF1, 0xCE, 0xC2, 0x2E, 0x24, 0x46, 0x95,
    0x09, 0x19, 0xCA, 0xC0, 0x46, 0xB2, 0xE5, 0x01, 0xBA, 0xD7, 0x4F, 0xF3,
    0xBF, 0xF6, 0x69, 0xAD, 0x99, 0x04, 0xFA, 0xA0, 0x07, 0x39, 0x0E, 0xE6,
    0xDF, 0x51, 0x47, 0x07, 0xC0, 0xE4, 0xA9, 0x5C, 0x4B, 0x94, 0xC5, 0x2F,
    0xB3, 0xA0, 0x30, 0x7F, 0xE7, 0x95, 0x6B, 0xB2, 0xAF, 0x32, 0x0D, 0xF1,
    0x8C, 0xD5, 0x6D, 0xCB, 0x7B, 0x47, 0xA7, 0x08, 0xAB, 0xCB, 0x27, 0xA3,
    0x4D, 0xCF, 0x4A, 0x5A, 0xF1, 0x05, 0xD1, 0xF8, 0x62, 0xC5, 0x10, 0x2A,
    0x74, 0x69, 0xAA, 0xE6, 0x4B, 0x96, 0xFB, 0x9B, 0xD8, 0x63, 0xE4, 0x58,
    0x66, 0xD3, 0xAD, 0x8A, 0x6E, 0xFF, 0x7B, 0x5E, 0xF9, 0xA5, 0x56, 0x1E,
    0x2D, 0x82, 0x31, 0x5B, 0xF0, 0xE2, 0x24, 0xE6, 0x41, 0x4A, 0x1F, 0xAE,
    0x13, 0x02, 0x03, 0x01, 0x00, 0x01,
};

unsigned char kEurekaRootCaSubjectDer[126] = {
    0x30, 0x7C, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08,
    0x0C, 0x0A, 0x43, 0x61, 0x6C, 0x69, 0x66, 0x6F, 0x72, 0x6E, 0x69, 0x61,
    0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x07, 0x0C, 0x0D, 0x4D,
    0x6F, 0x75, 0x6E, 0x74, 0x61, 0x69, 0x6E, 0x20, 0x56, 0x69, 0x65, 0x77,
    0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x0A, 0x47,
    0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20, 0x49, 0x6E, 0x63, 0x31, 0x12, 0x30,
    0x10, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x0C, 0x09, 0x47, 0x6F, 0x6F, 0x67,
    0x6C, 0x65, 0x20, 0x54, 0x56, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x0C, 0x0E, 0x45, 0x75, 0x72, 0x65, 0x6B, 0x61, 0x20, 0x52,
    0x6F, 0x6F, 0x74, 0x20, 0x43, 0x41,
};

unsigned char kEurekaRootCaSpkiDer[294] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0F, 0x00,
    0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xB9, 0x11, 0xD0,
    0xEA, 0x12, 0xDC, 0x32, 0xE1, 0xDF, 0x5C, 0x33, 0x6B, 0x19, 0x73, 0x1D,
    0x9D, 0x9E, 0xD0, 0x39, 0x76, 0xBF, 0xA5, 0x84, 0x09, 0xA6, 0xFD, 0x6E,
    0x6D, 0xE9, 0xDC, 0x8F, 0x36, 0x4E, 0xE9, 0x88, 0x02, 0xBD, 0x9F, 0xF4,
    0xE8, 0x44, 0xFD, 0x4C, 0xF5, 0x9A, 0x02, 0x56, 0x6A, 0x47, 0x2A, 0x63,
    0x6C, 0x58, 0x45, 0xCC, 0x7C, 0x66, 0x24, 0xDC, 0x79, 0x79, 0xC3, 0x2A,
    0xA4, 0xB2, 0x8B, 0xA0, 0xF7, 0xA2, 0xB5, 0xCD, 0x06, 0x7E, 0xDB, 0xBE,
    0xEC, 0x0C, 0x86, 0xF2, 0x0D, 0x24, 0x60, 0x74, 0x84, 0xCA, 0x29, 0x23,
    0x84, 0x02, 0xD8, 0xA7, 0xED, 0x3B, 0xF1, 0xEC, 0x26, 0x47, 0x54, 0xE3,
    0xB1, 0x2D, 0xE6, 0x64, 0x0F, 0xF6, 0x72, 0xC5, 0xE9, 0x98, 0x52, 0x17,
    0xC0, 0xFC, 0xF2, 0x2C, 0x20, 0xC8, 0x40, 0xF8, 0x47, 0xC9, 0x32, 0x9E,
    0x3B, 0x97, 0xB1, 0x8B, 0xF5, 0x98, 0x24, 0x70, 0x63, 0x66, 0x19, 0xC1,
    0x52, 0xE8, 0x04, 0x05, 0x3D, 0x5F, 0x8D, 0xBC, 0xD8, 0x4B, 0xAF, 0x77,
    0x98, 0x6F, 0x1F, 0x78, 0xD1, 0xB6, 0x50, 0x27, 0x4D, 0xE4, 0xEC, 0x14,
    0x69, 0x67, 0x1F, 0x58, 0xAF, 0xA9, 0xA0, 0x11, 0x26, 0x3C, 0x94, 0x32,
    0x07, 0x7F, 0xD7, 0xE9, 0x69, 0x1F, 0xAE, 0x3F, 0x4F, 0x63, 0x8A, 0x8F,
    0x89, 0xD6, 0xF2, 0x19, 0x78, 0x5C, 0x21, 0x8E, 0xB1, 0xB6, 0x57, 0xD8,
    0xC0, 0xE1, 0xEE, 0x7D, 0x6E, 0xDD, 0xF1, 0x3A, 0x0A, 0x6A, 0xF1, 0xBA,
    0xFF, 0xF9, 0x83, 0x2F, 0xDC, 0xB5, 0xA4, 0x20, 0x17, 0x63, 0x36, 0xEF,
    0xC8, 0x62, 0x19, 0xCC, 0x56, 0xCE, 0xB2, 0xEA, 0x31, 0x89, 0x4B, 0x78,
    0x58, 0xC1, 0xBF, 0x03, 0x13, 0x99, 0xE0, 0x12, 0xF2, 0x88, 0xAA, 0x9B,
    0x94, 0xDA, 0xDD, 0x76, 0x79, 0x17, 0x1E, 0x34, 0xD1, 0x0A, 0xC4, 0x07,
    0x45, 0x02, 0x03, 0x01, 0x00, 0x01,
};

// Helper function that creates and initializes a TrustAnchor struct given
// arrays for the subject's DER and the SPKI's DER.
template <size_t SubjectSize, size_t SpkiSize>
net::TrustAnchor CreateTrustAnchor(const uint8_t (&subject)[SubjectSize],
                                   const uint8_t (&spki)[SpkiSize]) {
  net::TrustAnchor anchor;
  anchor.name = std::string(subject, subject + SubjectSize);
  anchor.spki = std::string(spki, spki + SpkiSize);
  return anchor;
}

// Creates a trust store with the two Cast roots.
//
// TODO(eroman): The root certificates themselves are not included in the trust
// store (just their subject/SPKI). The problem with this approach is any
// restrictions encoded in their (like path length, or policy) are not known
// when verifying, and hence not enforced.
net::TrustStore CreateCastTrustStore() {
  net::TrustStore store;
  store.anchors.push_back(
      CreateTrustAnchor(kEurekaRootCaSubjectDer, kEurekaRootCaSpkiDer));
  store.anchors.push_back(
      CreateTrustAnchor(kCastRootCaSubjectDer, kCastRootCaSpkiDer));
  return store;
}

using ExtensionsMap = std::map<net::der::Input, net::ParsedExtension>;

// Helper that looks up an extension by OID given a map of extensions.
bool GetExtensionValue(const ExtensionsMap& extensions,
                       const net::der::Input& oid,
                       net::der::Input* value) {
  auto it = extensions.find(oid);
  if (it == extensions.end())
    return false;
  *value = it->second.value;
  return true;
}

// Returns the OID for the Audio-Only Cast policy
// (1.3.6.1.4.1.11129.2.5.2) in DER form.
net::der::Input AudioOnlyPolicyOid() {
  static const uint8_t kAudioOnlyPolicy[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                             0xD6, 0x79, 0x02, 0x05, 0x02};
  return net::der::Input(kAudioOnlyPolicy);
}

// Cast certificates rely on RSASSA-PKCS#1 v1.5 with SHA-1 for signatures.
//
// The following signature policy specifies which signature algorithms (and key
// sizes) are acceptable. It is used when verifying a chain of certificates, as
// well as when verifying digital signature using the target certificate's
// SPKI.
//
// This particular policy allows for:
//   * ECDSA, RSA-SSA, and RSA-PSS
//   * Supported EC curves: P-256, P-384, P-521.
//   * Hashes: All SHA hashes including SHA-1 (despite being known weak).
//   * RSA keys must have a modulus at least 2048-bits long.
scoped_ptr<net::SignaturePolicy> CreateCastSignaturePolicy() {
  return make_scoped_ptr(new net::SimpleSignaturePolicy(2048));
}

// TODO(eroman): Remove "2" from the name once the old approach is no longer
// used.
class CertVerificationContextImpl2 : public CertVerificationContext {
 public:
  // Save a copy of the passed in public key (DER) and common name (text).
  CertVerificationContextImpl2(const net::der::Input& spki,
                               const base::StringPiece& common_name)
      : spki_(spki.AsString()), common_name_(common_name.as_string()) {}

  VerificationResult VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data) const override {
    // This code assumes the signature algorithm was RSASSA PKCS#1 v1.5 with
    // SHA-1.
    // TODO(eroman): Is it possible to use other hash algorithms?
    auto signature_algorithm =
        net::SignatureAlgorithm::CreateRsaPkcs1(net::DigestAlgorithm::Sha1);

    // Use the same policy as was used for verifying signatures in
    // certificates. This will ensure for instance that the key used is at
    // least 2048-bits long.
    auto signature_policy = CreateCastSignaturePolicy();

    bool success = net::VerifySignedData(
        *signature_algorithm, net::der::Input(data),
        net::der::BitString(net::der::Input(signature), 0),
        net::der::Input(&spki_), signature_policy.get());

    if (success)
      return VerificationResult();

    // TODO(eroman): This error is ambiguous. Could have failed for a number of
    // reasons, not just invalid signatures (i.e. SPKI parsing, signature
    // policy, etc).
    return VerificationResult("Signature verification failed.",
                              VerificationResult::ERROR_SIGNATURE_INVALID);
  }

  std::string GetCommonName() const override { return common_name_; }

 private:
  std::string spki_;
  std::string common_name_;
};

// Helper that extracts the Common Name from a certificate's subject field. On
// success |common_name| contains the text for the attribute (unescaped, so
// will depend on the encoding used, but for Cast device certs it should
// be ASCII).
bool GetCommonNameFromSubject(const net::der::Input& subject_tlv,
                              std::string* common_name) {
  net::RDNSequence rdn_sequence;
  if (!net::ParseName(subject_tlv, &rdn_sequence))
    return false;

  for (const net::RelativeDistinguishedName& rdn : rdn_sequence) {
    for (const auto& atv : rdn) {
      if (atv.type == net::TypeCommonNameOid()) {
        return atv.ValueAsStringUnsafe(common_name);
      }
    }
  }
  return false;
}

// Returns true if the extended key usage list |ekus| contains client auth.
bool HasClientAuth(const std::vector<net::der::Input>& ekus) {
  for (const auto& oid : ekus) {
    if (oid == net::ClientAuth())
      return true;
  }
  return false;
}

// Checks properties on the target certificate.
//
//   * The Key Usage must include Digital Signature
//   * THe Extended Key Usage must includ TLS Client Auth
//   * May have the policy 1.3.6.1.4.1.11129.2.5.2 to indicate it
//     is an audio-only device.
WARN_UNUSED_RESULT bool CheckTargetCertificate(
    const net::der::Input& cert_der,
    scoped_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy) {
  // TODO(eroman): Simplify this. The certificate chain verification
  // function already parses this stuff, awkward to re-do it here.

  net::ParsedCertificate cert;
  if (!net::ParseCertificate(cert_der, &cert))
    return false;

  net::ParsedTbsCertificate tbs;
  if (!net::ParseTbsCertificate(cert.tbs_certificate_tlv, &tbs))
    return false;

  // Get the extensions.
  if (!tbs.has_extensions)
    return false;
  ExtensionsMap extensions;
  if (!net::ParseExtensions(tbs.extensions_tlv, &extensions))
    return false;

  net::der::Input extension_value;

  // Get the Key Usage extension.
  if (!GetExtensionValue(extensions, net::KeyUsageOid(), &extension_value))
    return false;
  net::der::BitString key_usage;
  if (!net::ParseKeyUsage(extension_value, &key_usage))
    return false;

  // Ensure Key Usage contains digitalSignature.
  if (!key_usage.AssertsBit(net::KEY_USAGE_BIT_DIGITAL_SIGNATURE))
    return false;

  // Get the Extended Key Usage extension.
  if (!GetExtensionValue(extensions, net::ExtKeyUsageOid(), &extension_value))
    return false;
  std::vector<net::der::Input> ekus;
  if (!net::ParseEKUExtension(extension_value, &ekus))
    return false;

  // Ensure Extended Key Usage contains client auth.
  if (!HasClientAuth(ekus))
    return false;

  // Check for an optional audio-only policy extension.
  *policy = CastDeviceCertPolicy::NONE;
  if (GetExtensionValue(extensions, net::CertificatePoliciesOid(),
                        &extension_value)) {
    std::vector<net::der::Input> policies;
    if (!net::ParseCertificatePoliciesExtension(extension_value, &policies))
      return false;

    // Look for an audio-only policy. Disregard any other policy found.
    if (std::find(policies.begin(), policies.end(), AudioOnlyPolicyOid()) !=
        policies.end()) {
      *policy = CastDeviceCertPolicy::AUDIO_ONLY;
    }
  }

  // Get the Common Name for the certificate.
  std::string common_name;
  if (!GetCommonNameFromSubject(tbs.subject_tlv, &common_name))
    return false;

  context->reset(new CertVerificationContextImpl2(tbs.spki_tlv, common_name));
  return true;
}

class CertVerificationContextImpl : public CertVerificationContext {
 public:
  // Takes ownership of the passed-in x509 object
  explicit CertVerificationContextImpl(net::ScopedX509 x509)
      : x509_(std::move(x509)) {}

  VerificationResult VerifySignatureOverData(
      const base::StringPiece& signature,
      const base::StringPiece& data) const override {
    // Retrieve public key object.
    crypto::ScopedEVP_PKEY public_key(X509_get_pubkey(x509_.get()));
    if (!public_key) {
      return VerificationResult(
          "Failed to extract device certificate public key.",
          VerificationResult::ERROR_CERT_INVALID);
    }
    // Make sure the key is RSA.
    const int public_key_type = EVP_PKEY_id(public_key.get());
    if (public_key_type != EVP_PKEY_RSA) {
      return VerificationResult(
          std::string("Expected RSA key type for client certificate, got ") +
              base::IntToString(public_key_type) + " instead.",
          VerificationResult::ERROR_CERT_INVALID);
    }
    // Verify signature.
    const crypto::ScopedEVP_MD_CTX ctx(EVP_MD_CTX_create());
    if (!ctx ||
        !EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha1(), nullptr,
                              public_key.get()) ||
        !EVP_DigestVerifyUpdate(ctx.get(), data.data(), data.size()) ||
        !EVP_DigestVerifyFinal(
            ctx.get(), reinterpret_cast<const uint8_t*>(signature.data()),
            signature.size())) {
      return VerificationResult("Signature verification failed.",
                                VerificationResult::ERROR_SIGNATURE_INVALID);
    }
    return VerificationResult();
  }

  std::string GetCommonName() const override {
    int common_name_length = X509_NAME_get_text_by_NID(
        x509_->cert_info->subject, NID_commonName, NULL, 0);
    if (common_name_length < 0)
      return std::string();
    std::string common_name;
    common_name_length = X509_NAME_get_text_by_NID(
        x509_->cert_info->subject, NID_commonName,
        base::WriteInto(&common_name,
                        static_cast<size_t>(common_name_length) + 1),
        common_name_length + 1);
    if (common_name_length < 0)
      return std::string();
    return common_name;
  }

 private:
  net::ScopedX509 x509_;
};

// Converts a base::Time::Exploded to a net::der::GeneralizedTime.
net::der::GeneralizedTime ConvertExplodedTime(
    const base::Time::Exploded& exploded) {
  net::der::GeneralizedTime result;
  result.year = exploded.year;
  result.month = exploded.month;
  result.day = exploded.day_of_month;
  result.hours = exploded.hour;
  result.minutes = exploded.minute;
  result.seconds = exploded.second;
  return result;
}

}  // namespace

VerificationResult::VerificationResult() : VerificationResult("", ERROR_NONE) {}

VerificationResult::VerificationResult(const std::string& in_error_message,
                                       ErrorType in_error_type)
    : error_type(in_error_type), error_message(in_error_message) {}

VerificationResult VerifyDeviceCert(
    const base::StringPiece& device_cert,
    const std::vector<std::string>& ica_certs,
    scoped_ptr<CertVerificationContext>* context) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // If the list of intermediates is empty then use kPublicKeyICA1 as
  // the trusted CA (legacy case).
  // Otherwise, use the first intermediate in the list as long as it
  // is in the allowed list of intermediates.
  base::StringPiece ica_public_key_der =
      (ica_certs.size() == 0)
          ? cast_channel::GetDefaultTrustedICAPublicKey()
          : cast_channel::GetTrustedICAPublicKey(ica_certs[0]);

  if (ica_public_key_der.empty()) {
    return VerificationResult(
        "Device certificate is not signed by a trusted CA",
        VerificationResult::ERROR_CERT_UNTRUSTED);
  }

  // Initialize the ICA public key.
  crypto::ScopedRSA ica_public_key_rsa(RSA_public_key_from_bytes(
      reinterpret_cast<const uint8_t*>(ica_public_key_der.data()),
      ica_public_key_der.size()));
  if (!ica_public_key_rsa) {
    return VerificationResult("Failed to import trusted public key.",
                              VerificationResult::ERROR_INTERNAL);
  }
  crypto::ScopedEVP_PKEY ica_public_key_evp(EVP_PKEY_new());
  if (!ica_public_key_evp ||
      !EVP_PKEY_set1_RSA(ica_public_key_evp.get(), ica_public_key_rsa.get())) {
    return VerificationResult("Failed to import trusted public key.",
                              VerificationResult::ERROR_INTERNAL);
  }

  // Parse the device certificate.
  const uint8_t* device_cert_der_ptr =
      reinterpret_cast<const uint8_t*>(device_cert.data());
  const uint8_t* device_cert_der_end = device_cert_der_ptr + device_cert.size();
  net::ScopedX509 device_cert_x509(
      d2i_X509(NULL, &device_cert_der_ptr, device_cert.size()));
  if (!device_cert_x509 || device_cert_der_ptr != device_cert_der_end) {
    return VerificationResult("Failed to parse device certificate.",
                              VerificationResult::ERROR_CERT_INVALID);
  }

  // Verify device certificate.
  if (X509_verify(device_cert_x509.get(), ica_public_key_evp.get()) != 1) {
    return VerificationResult(
        "Device certificate signature verification failed.",
        VerificationResult::ERROR_CERT_INVALID);
  }

  if (context)
    context->reset(
        new CertVerificationContextImpl(std::move(device_cert_x509)));

  return VerificationResult();
}

bool VerifyDeviceCert2(const std::vector<std::string>& certs,
                       const base::Time::Exploded& time,
                       scoped_ptr<CertVerificationContext>* context,
                       CastDeviceCertPolicy* policy) {
  // Initialize the trust store used for verifying Cast
  // device certificates.
  //
  // Performance: This code is re-building a TrustStore object each
  // time a chain needs to be verified rather than caching it, to
  // avoid memory bloat.
  auto trust_store = CreateCastTrustStore();

  // The underlying verification function expects a sequence of
  // der::Input, so wrap the data in it (cheap).
  std::vector<net::der::Input> input_chain;
  for (const auto& cert : certs)
    input_chain.push_back(net::der::Input(&cert));

  // Use a signature policy compatible with Cast's PKI.
  auto signature_policy = CreateCastSignaturePolicy();

  // Do RFC 5280 compatible certificate verification using the two Cast
  // trust anchors and Cast signature policy.
  if (!net::VerifyCertificateChain(input_chain, trust_store,
                                   signature_policy.get(),
                                   ConvertExplodedTime(time))) {
    return false;
  }

  // Check properties of the leaf certificate (key usage, policy), and construct
  // a CertVerificationContext that uses its public key.
  return CheckTargetCertificate(input_chain[0], context, policy);
}

scoped_ptr<CertVerificationContext> CertVerificationContextImplForTest(
    const base::StringPiece& spki) {
  // Use a bogus CommonName, since this is just exposed for testing signature
  // verification by unittests.
  return make_scoped_ptr(
      new CertVerificationContextImpl2(net::der::Input(spki), "CommonName"));
}

bool SetTrustedCertificateAuthoritiesForTest(const std::string& keys,
                                             const std::string& signature) {
  return extensions::api::cast_channel::SetTrustedCertificateAuthorities(
      keys, signature);
}

}  // namespace cast_crypto
}  // namespace api
}  // namespace extensions
