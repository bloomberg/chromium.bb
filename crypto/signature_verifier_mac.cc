// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_verifier.h"

#include <stdlib.h>

#include "base/logging.h"
#include "crypto/cssm_init.h"

namespace crypto {

SignatureVerifier::SignatureVerifier() : sig_handle_(0) {
  EnsureCSSMInit();
}

SignatureVerifier::~SignatureVerifier() {
  Reset();
}

bool SignatureVerifier::VerifyInit(const uint8* signature_algorithm,
                                   int signature_algorithm_len,
                                   const uint8* signature,
                                   int signature_len,
                                   const uint8* public_key_info,
                                   int public_key_info_len) {
  signature_.assign(signature, signature + signature_len);
  public_key_info_.assign(public_key_info,
                          public_key_info + public_key_info_len);

  CSSM_ALGORITHMS key_alg = CSSM_ALGID_RSA;  // TODO(wtc): hardcoded.

  memset(&public_key_, 0, sizeof(public_key_));
  public_key_.KeyData.Data = const_cast<uint8*>(&public_key_info_[0]);
  public_key_.KeyData.Length = public_key_info_.size();
  public_key_.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
  public_key_.KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
  public_key_.KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_X509;
  public_key_.KeyHeader.AlgorithmId = key_alg;
  public_key_.KeyHeader.KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
  public_key_.KeyHeader.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
  public_key_.KeyHeader.KeyUsage = CSSM_KEYUSE_VERIFY;
  CSSM_KEY_SIZE key_size;
  CSSM_RETURN crtn;
  crtn = CSSM_QueryKeySizeInBits(GetSharedCSPHandle(), NULL,
                                 &public_key_, &key_size);
  if (crtn) {
    NOTREACHED() << "CSSM_QueryKeySizeInBits failed: " << crtn;
    return false;
  }
  public_key_.KeyHeader.LogicalKeySizeInBits = key_size.LogicalKeySizeInBits;

  // TODO(wtc): decode signature_algorithm...
  CSSM_ALGORITHMS sig_alg = CSSM_ALGID_SHA1WithRSA;

  crtn = CSSM_CSP_CreateSignatureContext(GetSharedCSPHandle(), sig_alg, NULL,
                                         &public_key_, &sig_handle_);
  if (crtn) {
    NOTREACHED();
    return false;
  }
  crtn = CSSM_VerifyDataInit(sig_handle_);
  if (crtn) {
    NOTREACHED();
    return false;
  }
  return true;
}

void SignatureVerifier::VerifyUpdate(const uint8* data_part,
                                     int data_part_len) {
  CSSM_DATA data;
  data.Data = const_cast<uint8*>(data_part);
  data.Length = data_part_len;
  CSSM_RETURN crtn = CSSM_VerifyDataUpdate(sig_handle_, &data, 1);
  DCHECK(crtn == CSSM_OK);
}

bool SignatureVerifier::VerifyFinal() {
  CSSM_DATA sig;
  sig.Data = const_cast<uint8*>(&signature_[0]);
  sig.Length = signature_.size();
  CSSM_RETURN crtn = CSSM_VerifyDataFinal(sig_handle_, &sig);
  Reset();

  // crtn is CSSMERR_CSP_VERIFY_FAILED if signature verification fails.
  return (crtn == CSSM_OK);
}

void SignatureVerifier::Reset() {
  CSSM_RETURN crtn;
  if (sig_handle_) {
    crtn = CSSM_DeleteContext(sig_handle_);
    DCHECK(crtn == CSSM_OK);
    sig_handle_ = 0;
  }
  signature_.clear();

  // Can't call CSSM_FreeKey on public_key_ because we constructed
  // public_key_ manually.
}

}  // namespace crypto

