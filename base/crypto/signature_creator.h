// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_SIGNATURE_CREATOR_H_
#define BASE_CRYPTO_SIGNATURE_CREATOR_H_

#include "build/build_config.h"

#if defined(USE_NSS)
// Forward declaration.
struct SGNContextStr;
#elif defined(OS_MACOSX)
#include <Security/cssm.h>
#elif defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#endif

#include <vector>

#include "base/basictypes.h"
#include "base/crypto/rsa_private_key.h"

namespace base {

// Signs data using a bare private key (as opposed to a full certificate).
// Currently can only sign data using SHA-1 with RSA encryption.
class SignatureCreator {
 public:
  // Create an instance. The caller must ensure that the provided PrivateKey
  // instance outlives the created SignatureCreator.
  static SignatureCreator* Create(RSAPrivateKey* key);

  ~SignatureCreator();

  // Update the signature with more data.
  bool Update(const uint8* data_part, int data_part_len);

  // Finalize the signature.
  bool Final(std::vector<uint8>* signature);

 private:
  // Private constructor. Use the Create() method instead.
  SignatureCreator();

  RSAPrivateKey* key_;

#if defined(USE_NSS)
  SGNContextStr* sign_context_;
#elif defined(OS_MACOSX)
  CSSM_CSP_HANDLE csp_handle_;	
  CSSM_CC_HANDLE sig_handle_;
#elif defined(OS_WIN)
  HCRYPTHASH hash_object_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SignatureCreator);
};

}  // namespace base

#endif  // BASE_CRYPTO_SIGNATURE_CREATOR_H_
