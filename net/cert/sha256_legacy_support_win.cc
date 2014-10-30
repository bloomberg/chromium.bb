// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/sha256_legacy_support_win.h"

namespace net {

namespace sha256_interception {

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

}  // namespace sha256_interception

}  // namespace net