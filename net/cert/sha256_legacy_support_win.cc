// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sha256_legacy_support_win.h"

#include <windows.h>
#include <wincrypt.h>

#include <cert.h>
#include <keyhi.h>
#include <secoid.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/win/windows_version.h"
#include "crypto/scoped_nss_types.h"

namespace net {

namespace sha256_interception {

namespace {

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

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CERTSignedData signed_data;
  memset(&signed_data, 0, sizeof(signed_data));

  // Attempt to decode the subject using the generic "Signed Data" template,
  // which all of the supported subject types match. If the signature
  // algorithm is RSA with one of the SHA-2 algorithms supported by NSS
  // (excluding SHA-224, which is pointless), then defer to the NSS
  // implementation. Otherwise, fall back and let the OS handle it (e.g.
  // in case there are any algorithm policies in effect).
  if (!subject_signature.empty()) {
    SECItem subject_sig_item;
    subject_sig_item.data = const_cast<unsigned char*>(
        reinterpret_cast<const unsigned char*>(subject_signature.data()));
    subject_sig_item.len = subject_signature.size();
    SECStatus rv = SEC_QuickDERDecodeItem(
        arena.get(), &signed_data, SEC_ASN1_GET(CERT_SignedDataTemplate),
        &subject_sig_item);
    if (rv == SECSuccess) {
      SECOidTag signature_alg =
          SECOID_GetAlgorithmTag(&signed_data.signatureAlgorithm);
      if (signature_alg == SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION ||
          signature_alg == SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION ||
          signature_alg == SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION) {
        should_intercept = true;
      }
    }
  }

  if (!should_intercept) {
    return original_func(provider, encoding_type, subject_type, subject_data,
                         issuer_type, issuer_data, flags, extra);
  }

  // Rather than attempting to synthesize a CERTSubjectPublicKeyInfo by hand,
  // just force the OS to do an ASN.1 encoding and then decode it back into
  // NSS. This is silly for performance, but safest for consistency.
  PCERT_PUBLIC_KEY_INFO issuer_public_key =
      GetIssuerPublicKey(issuer_type, issuer_data);
  if (!issuer_public_key) {
    SetLastError(static_cast<DWORD>(NTE_BAD_ALGID));
    return FALSE;
  }

  unsigned char* issuer_spki_data = NULL;
  DWORD issuer_spki_len = 0;
  if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
                           issuer_public_key, CRYPT_ENCODE_ALLOC_FLAG, NULL,
                           &issuer_spki_data, &issuer_spki_len)) {
    return FALSE;
  }

  SECItem nss_issuer_spki;
  nss_issuer_spki.data = issuer_spki_data;
  nss_issuer_spki.len = issuer_spki_len;
  CERTSubjectPublicKeyInfo* spki =
      SECKEY_DecodeDERSubjectPublicKeyInfo(&nss_issuer_spki);
  ::LocalFree(issuer_spki_data);
  if (!spki) {
    SetLastError(static_cast<DWORD>(NTE_BAD_ALGID));
    return FALSE;
  }

  // Attempt to actually verify the signed data. If it fails, synthesize the
  // failure as a generic "bad signature" and let CryptoAPI handle the rest.
  SECStatus rv = CERT_VerifySignedDataWithPublicKeyInfo(
      &signed_data, spki, NULL);
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (rv != SECSuccess) {
    SetLastError(static_cast<DWORD>(NTE_BAD_SIGNATURE));
    return FALSE;
  }
  return TRUE;
}

}  // namespace sha256_interception

}  // namespace net