// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_AES_128_GCM_12_ENCRYPTER_H_
#define NET_QUIC_CRYPTO_AES_128_GCM_12_ENCRYPTER_H_

#include <string>

#include "base/compiler_specific.h"
#include "net/quic/crypto/quic_encrypter.h"

#if defined(USE_OPENSSL)
#include "net/quic/crypto/scoped_evp_aead_ctx.h"
#endif

namespace net {

namespace test {
class Aes128Gcm12EncrypterPeer;
}  // namespace test

// An Aes128Gcm12Encrypter is a QuicEncrypter that implements the
// AEAD_AES_128_GCM_12 algorithm specified in RFC 5282. Create an instance by
// calling QuicEncrypter::Create(kAESG).
//
// It uses an authentication tag of 12 bytes (96 bits). The fixed prefix
// of the nonce is four bytes.
class NET_EXPORT_PRIVATE Aes128Gcm12Encrypter : public QuicEncrypter {
 public:
  enum {
    // Authentication tags are truncated to 96 bits.
    kAuthTagSize = 12,
  };

  Aes128Gcm12Encrypter();
  virtual ~Aes128Gcm12Encrypter();

  // QuicEncrypter implementation
  virtual bool SetKey(base::StringPiece key) OVERRIDE;
  virtual bool SetNoncePrefix(base::StringPiece nonce_prefix) OVERRIDE;
  virtual bool Encrypt(base::StringPiece nonce,
                       base::StringPiece associated_data,
                       base::StringPiece plaintext,
                       unsigned char* output) OVERRIDE;
  virtual QuicData* EncryptPacket(QuicPacketSequenceNumber sequence_number,
                                  base::StringPiece associated_data,
                                  base::StringPiece plaintext) OVERRIDE;
  virtual size_t GetKeySize() const OVERRIDE;
  virtual size_t GetNoncePrefixSize() const OVERRIDE;
  virtual size_t GetMaxPlaintextSize(size_t ciphertext_size) const OVERRIDE;
  virtual size_t GetCiphertextSize(size_t plaintext_size) const OVERRIDE;
  virtual base::StringPiece GetKey() const OVERRIDE;
  virtual base::StringPiece GetNoncePrefix() const OVERRIDE;

 private:
  // The 128-bit AES key.
  unsigned char key_[16];
  // The nonce prefix.
  unsigned char nonce_prefix_[4];

#if defined(USE_OPENSSL)
  ScopedEVPAEADCtx ctx_;
#endif
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_AES_128_GCM_12_ENCRYPTER_H_
