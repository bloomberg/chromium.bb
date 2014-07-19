// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_NSS_UTIL_NSS_H_
#define CONTENT_CHILD_WEBCRYPTO_NSS_UTIL_NSS_H_

#include <keythi.h>
#include <pkcs11t.h>
#include <seccomon.h>
#include <secmodt.h>

#include "base/lazy_instance.h"

namespace content {

namespace webcrypto {

class CryptoData;

SECItem MakeSECItemForBuffer(const CryptoData& buffer);
enum EncryptOrDecrypt { ENCRYPT, DECRYPT };

CryptoData SECItemToCryptoData(const SECItem& item);

// Signature for PK11_Encrypt and PK11_Decrypt.
typedef SECStatus (*PK11_EncryptDecryptFunction)(PK11SymKey*,
                                                 CK_MECHANISM_TYPE,
                                                 SECItem*,
                                                 unsigned char*,
                                                 unsigned int*,
                                                 unsigned int,
                                                 const unsigned char*,
                                                 unsigned int);

// Signature for PK11_PubEncrypt
typedef SECStatus (*PK11_PubEncryptFunction)(SECKEYPublicKey*,
                                             CK_MECHANISM_TYPE,
                                             SECItem*,
                                             unsigned char*,
                                             unsigned int*,
                                             unsigned int,
                                             const unsigned char*,
                                             unsigned int,
                                             void*);

// Signature for PK11_PrivDecrypt
typedef SECStatus (*PK11_PrivDecryptFunction)(SECKEYPrivateKey*,
                                              CK_MECHANISM_TYPE,
                                              SECItem*,
                                              unsigned char*,
                                              unsigned int*,
                                              unsigned int,
                                              const unsigned char*,
                                              unsigned int);

// Singleton that detects whether or not AES-GCM and
// RSA-OAEP are supported by the version of NSS being used.
// On non-Linux platforms, Chromium embedders ship with a
// fixed version of NSS, and these are always available.
// However, on Linux (and ChromeOS), NSS is provided by the
// system, and thus not all algorithms may be available
// or be safe to use.
class NssRuntimeSupport {
 public:
  bool IsAesGcmSupported() const {
    return pk11_encrypt_func_ && pk11_decrypt_func_;
  }

  bool IsRsaOaepSupported() const {
    return pk11_pub_encrypt_func_ && pk11_priv_decrypt_func_ &&
           internal_slot_does_oaep_;
  }

  // Returns NULL if unsupported.
  PK11_EncryptDecryptFunction pk11_encrypt_func() const {
    return pk11_encrypt_func_;
  }

  // Returns NULL if unsupported.
  PK11_EncryptDecryptFunction pk11_decrypt_func() const {
    return pk11_decrypt_func_;
  }

  // Returns NULL if unsupported.
  PK11_PubEncryptFunction pk11_pub_encrypt_func() const {
    return pk11_pub_encrypt_func_;
  }

  // Returns NULL if unsupported.
  PK11_PrivDecryptFunction pk11_priv_decrypt_func() const {
    return pk11_priv_decrypt_func_;
  }

  static NssRuntimeSupport* Get();

 private:
  friend struct base::DefaultLazyInstanceTraits<NssRuntimeSupport>;

  NssRuntimeSupport();

  PK11_EncryptDecryptFunction pk11_encrypt_func_;
  PK11_EncryptDecryptFunction pk11_decrypt_func_;
  PK11_PubEncryptFunction pk11_pub_encrypt_func_;
  PK11_PrivDecryptFunction pk11_priv_decrypt_func_;
  bool internal_slot_does_oaep_;
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_NSS_UTIL_NSS_H_
