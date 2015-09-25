// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sha256_legacy_support_win.h"

#include <openssl/asn1.h>
#include <openssl/bytestring.h>
#include <openssl/evp.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "crypto/scoped_openssl_types.h"

namespace net {

namespace sha256_interception {

namespace {

using ScopedX509_ALGOR = crypto::ScopedOpenSSL<X509_ALGOR, X509_ALGOR_free>;

bool IsSupportedSubjectType(DWORD subject_type) {
  switch (subject_type) {
    case CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB:
    case CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT:
    case CRYPT_VERIFY_CERT_SIGN_SUBJECT_CRL:
      return true;
  }
  return false;
}

bool IsSupportedIssuerType(DWORD issuer_type) {
  switch (issuer_type) {
    case CRYPT_VERIFY_CERT_SIGN_ISSUER_PUBKEY:
    case CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT:
    case CRYPT_VERIFY_CERT_SIGN_ISSUER_CHAIN:
      return true;
  }
  return false;
}

base::StringPiece GetSubjectSignature(DWORD subject_type,
                                      void* subject_data) {
  switch (subject_type) {
    case CRYPT_VERIFY_CERT_SIGN_SUBJECT_BLOB: {
      CRYPT_DATA_BLOB* data_blob =
          reinterpret_cast<CRYPT_DATA_BLOB*>(subject_data);
      return base::StringPiece(reinterpret_cast<char*>(data_blob->pbData),
                               data_blob->cbData);
    }
    case CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT: {
      PCCERT_CONTEXT subject_cert =
          reinterpret_cast<PCCERT_CONTEXT>(subject_data);
      return base::StringPiece(
          reinterpret_cast<char*>(subject_cert->pbCertEncoded),
          subject_cert->cbCertEncoded);
    }
    case CRYPT_VERIFY_CERT_SIGN_SUBJECT_CRL: {
      PCCRL_CONTEXT subject_crl =
          reinterpret_cast<PCCRL_CONTEXT>(subject_data);
      return base::StringPiece(
          reinterpret_cast<char*>(subject_crl->pbCrlEncoded),
          subject_crl->cbCrlEncoded);
    }
  }
  return base::StringPiece();
}

PCERT_PUBLIC_KEY_INFO GetIssuerPublicKey(DWORD issuer_type,
                                         void* issuer_data) {
  switch (issuer_type) {
    case CRYPT_VERIFY_CERT_SIGN_ISSUER_PUBKEY:
      return reinterpret_cast<PCERT_PUBLIC_KEY_INFO>(issuer_data);
    case CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT: {
      PCCERT_CONTEXT cert = reinterpret_cast<PCCERT_CONTEXT>(issuer_data);
      return &cert->pCertInfo->SubjectPublicKeyInfo;
    }
    case CRYPT_VERIFY_CERT_SIGN_ISSUER_CHAIN: {
      PCCERT_CHAIN_CONTEXT chain =
          reinterpret_cast<PCCERT_CHAIN_CONTEXT>(issuer_data);
      PCCERT_CONTEXT cert = chain->rgpChain[0]->rgpElement[0]->pCertContext;
      return &cert->pCertInfo->SubjectPublicKeyInfo;
    }
  }
  return NULL;
}

// Parses |subject_signature| and writes the components into |*out_tbs_data|,
// |*out_algor|, and |*out_signature|. The BIT STRING in the signature must be
// a multiple of 8 bits. |*out_signature| will have the padding byte removed.
// It returns true on success and false on failure.
bool ParseSubjectSignature(const base::StringPiece& subject_signature,
                           CBS* out_tbs_data,
                           ScopedX509_ALGOR* out_algor,
                           CBS* out_signature) {
  CBS cbs, sequence, tbs_data, algorithm, signature;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(subject_signature.data()),
           subject_signature.size());
  if (!CBS_get_asn1(&cbs, &sequence, CBS_ASN1_SEQUENCE) || CBS_len(&cbs) != 0 ||
      !CBS_get_any_asn1_element(&sequence, &tbs_data, nullptr, nullptr) ||
      !CBS_get_asn1_element(&sequence, &algorithm, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&sequence, &signature, CBS_ASN1_BITSTRING) ||
      CBS_len(&sequence) != 0) {
    return false;
  }

  // Decode the algorithm.
  const uint8_t* ptr = CBS_data(&algorithm);
  ScopedX509_ALGOR algor(d2i_X509_ALGOR(NULL, &ptr, CBS_len(&algorithm)));
  if (!algor || ptr != CBS_data(&algorithm) + CBS_len(&algorithm))
    return false;

  // An ASN.1 BIT STRING is encoded with a leading byte denoting the number of
  // padding bits. All supported signature algorithms output octets, so the
  // leading byte must be zero.
  uint8_t padding;
  if (!CBS_get_u8(&signature, &padding) || padding != 0)
    return false;

  *out_tbs_data = tbs_data;
  *out_algor = algor.Pass();
  *out_signature = signature;
  return true;
}

}  // namespace

BOOL CryptVerifyCertificateSignatureExHook(
    CryptVerifyCertificateSignatureExFunc original_func,
    HCRYPTPROV_LEGACY provider,
    DWORD encoding_type,
    DWORD subject_type,
    void* subject_data,
    DWORD issuer_type,
    void* issuer_data,
    DWORD flags,
    void* extra) {
  CHECK(original_func);

  // Only intercept if the arguments are supported.
  if (provider != NULL || (encoding_type != X509_ASN_ENCODING) ||
      !IsSupportedSubjectType(subject_type) || subject_data == NULL ||
      !IsSupportedIssuerType(issuer_type) || issuer_data == NULL) {
    return original_func(provider, encoding_type, subject_type, subject_data,
                         issuer_type, issuer_data, flags, extra);
  }

  base::StringPiece subject_signature =
      GetSubjectSignature(subject_type, subject_data);
  bool should_intercept = false;

  // Parse out the data, AlgorithmIdentifier, and signature.
  CBS tbs_data, signature;
  ScopedX509_ALGOR algor;
  if (ParseSubjectSignature(subject_signature, &tbs_data, &algor, &signature)) {
    // If the signature algorithm is RSA with one of the SHA-2 algorithms
    // supported by BoringSSL (excluding SHA-224, which is pointless), then
    // defer to the BoringSSL implementation. Otherwise, fall back and let the
    // OS handle it (e.g. in case there are any algorithm policies in effect).
    int nid = OBJ_obj2nid(algor->algorithm);
    if (nid == NID_sha256WithRSAEncryption ||
        nid == NID_sha384WithRSAEncryption ||
        nid == NID_sha512WithRSAEncryption) {
      should_intercept = true;
    }
  }

  if (!should_intercept) {
    return original_func(provider, encoding_type, subject_type, subject_data,
                         issuer_type, issuer_data, flags, extra);
  }

  // Rather than attempting to synthesize an EVP_PKEY by hand, just force the
  // OS to do an ASN.1 encoding and then decode it back into BoringSSL. This
  // is silly for performance, but safest for consistency.
  PCERT_PUBLIC_KEY_INFO issuer_public_key =
      GetIssuerPublicKey(issuer_type, issuer_data);
  if (!issuer_public_key) {
    SetLastError(static_cast<DWORD>(NTE_BAD_ALGID));
    return FALSE;
  }

  uint8_t* issuer_spki_data = NULL;
  DWORD issuer_spki_len = 0;
  if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
                           issuer_public_key, CRYPT_ENCODE_ALLOC_FLAG, NULL,
                           &issuer_spki_data, &issuer_spki_len)) {
    return FALSE;
  }

  const uint8_t* ptr = issuer_spki_data;
  crypto::ScopedEVP_PKEY pubkey(d2i_PUBKEY(NULL, &ptr, issuer_spki_len));
  if (!pubkey.get() || ptr != issuer_spki_data + issuer_spki_len) {
    ::LocalFree(issuer_spki_data);
    SetLastError(static_cast<DWORD>(NTE_BAD_ALGID));
    return FALSE;
  }
  ::LocalFree(issuer_spki_data);

  crypto::ScopedEVP_MD_CTX md_ctx(EVP_MD_CTX_create());
  if (!EVP_DigestVerifyInitFromAlgorithm(md_ctx.get(), algor.get(),
                                         pubkey.get()) ||
      !EVP_DigestVerifyUpdate(md_ctx.get(), CBS_data(&tbs_data),
                              CBS_len(&tbs_data)) ||
      !EVP_DigestVerifyFinal(md_ctx.get(), CBS_data(&signature),
                             CBS_len(&signature))) {
    SetLastError(static_cast<DWORD>(NTE_BAD_SIGNATURE));
    return FALSE;
  }
  return TRUE;
}

}  // namespace sha256_interception

}  // namespace net
