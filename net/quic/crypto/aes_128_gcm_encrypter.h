// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_AES_128_GCM_ENCRYPTER_H_
#define NET_QUIC_CRYPTO_AES_128_GCM_ENCRYPTER_H_

#include <string>

#include "base/compiler_specific.h"
#include "net/quic/crypto/quic_encrypter.h"

namespace net {

namespace test {
class Aes128GcmEncrypterPeer;
}  // namespace test

// An Aes128GcmEncrypter is a QuicEncrypter that implements the
// AEAD_AES_128_GCM algorithm specified in RFC 5116. Create an instance by
// calling QuicEncrypter::Create(kAESG).
//
// It uses an authentication tag of 16 bytes (128 bits). The fixed prefix
// of the nonce is four bytes.
class NET_EXPORT_PRIVATE Aes128GcmEncrypter : public QuicEncrypter {
 public:
  virtual ~Aes128GcmEncrypter() {}

  // Returns true if the underlying crypto library supports AES GCM.
  static bool IsSupported();

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
  // The nonce, a concatenation of a four-byte fixed prefix and a 8-byte
  // packet sequence number.
  unsigned char nonce_[12];
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_AES_128_GCM_ENCRYPTER_H_
