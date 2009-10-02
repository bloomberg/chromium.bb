// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_RSA_PRIVATE_KEY_H_
#define BASE_CRYPTO_RSA_PRIVATE_KEY_H_

#include "build/build_config.h"

#if defined(USE_NSS)
#include <cryptoht.h>
#include <keythi.h>
#elif defined(OS_MACOSX)
#include <Security/cssm.h>
#elif defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#endif

#include <vector>

#include "base/basictypes.h"

namespace base {

// Encapsulates an RSA private key. Can be used to generate new keys, export
// keys to other formats, or to extract a public key.
class RSAPrivateKey {
 public:
  // Create a new random instance. Can return NULL if initialization fails.
  static RSAPrivateKey* Create(uint16 num_bits);

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  static RSAPrivateKey* CreateFromPrivateKeyInfo(
      const std::vector<uint8>& input);

  ~RSAPrivateKey();

#if defined(USE_NSS)
  SECKEYPrivateKey* key() { return key_; }
#elif defined(OS_WIN)
  HCRYPTPROV provider() { return provider_; }
  HCRYPTKEY key() { return key_; }
#elif defined(OS_MACOSX)
  CSSM_CSP_HANDLE csp_handle() { return csp_handle_; }
  CSSM_KEY_PTR key() { return &key_; }
#endif

  // Exports the private key to a PKCS #1 PrivateKey block.
  bool ExportPrivateKey(std::vector<uint8>* output);

  // Exports the public key to an X509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8>* output);

private:
  // Constructor is private. Use Create() or CreateFromPrivateKeyInfo()
  // instead.
  RSAPrivateKey();

#if defined(USE_NSS)
  SECKEYPrivateKey* key_;
  SECKEYPublicKey* public_key_;
#elif defined(OS_WIN)
  bool InitProvider();

  HCRYPTPROV provider_;
  HCRYPTKEY key_;
#elif defined(OS_MACOSX)
  CSSM_KEY key_;
  CSSM_CSP_HANDLE csp_handle_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RSAPrivateKey);
};

}  // namespace base

#endif  // BASE_CRYPTO_RSA_PRIVATE_KEY_H_
