// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/signature_creator.h"

#include <stdlib.h>

#include "base/crypto/cssm_init.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace base {

// static
SignatureCreator* SignatureCreator::Create(RSAPrivateKey* key) {
  scoped_ptr<SignatureCreator> result(new SignatureCreator);
  result->key_ = key;

  CSSM_RETURN crtn;
  crtn = CSSM_CSP_CreateSignatureContext(result->csp_handle_,
                                         CSSM_ALGID_SHA1WithRSA,
                                         NULL,
                                         key->key(),
                                         &result->sig_handle_);
  if (crtn) {
    NOTREACHED();
    return NULL;
  }

  crtn = CSSM_SignDataInit(result->sig_handle_);
  if (crtn) {
    NOTREACHED();
    return false;
  }

  return result.release();
}

SignatureCreator::SignatureCreator() : csp_handle_(0), sig_handle_(0) {
  EnsureCSSMInit();
  
  static CSSM_VERSION version = {2, 0};
  CSSM_RETURN crtn;
  crtn = CSSM_ModuleAttach(&gGuidAppleCSP, &version, &kCssmMemoryFunctions, 0,
                           CSSM_SERVICE_CSP, 0, CSSM_KEY_HIERARCHY_NONE,
                           NULL, 0, NULL, &csp_handle_);
  DCHECK(crtn == CSSM_OK);
}

SignatureCreator::~SignatureCreator() {
  CSSM_RETURN crtn;
  if (sig_handle_) {
    crtn = CSSM_DeleteContext(sig_handle_);
    DCHECK(crtn == CSSM_OK);
  }

  if (csp_handle_) {
    CSSM_RETURN crtn = CSSM_ModuleDetach(csp_handle_);
    DCHECK(crtn == CSSM_OK);
  }
}

bool SignatureCreator::Update(const uint8* data_part, int data_part_len) {
  CSSM_DATA data;
  data.Data = const_cast<uint8*>(data_part);
  data.Length = data_part_len;
  CSSM_RETURN crtn = CSSM_SignDataUpdate(sig_handle_, &data, 1);
  DCHECK(crtn == CSSM_OK);
  return true;
}

bool SignatureCreator::Final(std::vector<uint8>* signature) {
  CSSM_DATA sig;
  memset(&sig, 0, sizeof(CSSM_DATA)); // Allow CSSM allocate memory;
  CSSM_RETURN crtn = CSSM_SignDataFinal(sig_handle_, &sig);

  if (crtn) {
    NOTREACHED();
    return false;
  }

  signature->assign(sig.Data, sig.Data + sig.Length);
  kCssmMemoryFunctions.free_func(sig.Data, NULL); // Release data alloc'd
                                                  // by CSSM

  return true;
}

}  // namespace base
