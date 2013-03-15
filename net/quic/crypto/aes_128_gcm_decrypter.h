// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_AES_128_GCM_DECRYPTER_H_
#define NET_QUIC_CRYPTO_AES_128_GCM_DECRYPTER_H_

#include <string>

#include "base/compiler_specific.h"
#include "net/quic/crypto/quic_decrypter.h"

namespace net {

namespace test {
class Aes128GcmDecrypterPeer;
}  // namespace test

// An Aes128GcmDecrypter is a QuicDecrypter that implements the
// AEAD_AES_128_GCM algorithm specified in RFC 5116. Create an instance by
// calling QuicDecrypter::Create(kAESG).
//
// It uses an authentication tag of 16 bytes (128 bits). The fixed prefix
// of the nonce is four bytes.
class NET_EXPORT_PRIVATE Aes128GcmDecrypter : public QuicDecrypter {
 public:
  virtual ~Aes128GcmDecrypter() {}

  // Returns true if the underlying crypto library supports AES GCM.
  static bool IsSupported();

  // QuicDecrypter implementation
  virtual bool SetKey(base::StringPiece key) OVERRIDE;
  virtual bool SetNoncePrefix(base::StringPiece nonce_prefix) OVERRIDE;
  virtual QuicData* Decrypt(QuicPacketSequenceNumber sequence_number,
                            base::StringPiece associated_data,
                            base::StringPiece ciphertext) OVERRIDE;
  virtual base::StringPiece GetKey() const OVERRIDE;
  virtual base::StringPiece GetNoncePrefix() const OVERRIDE;

 private:
  friend class test::Aes128GcmDecrypterPeer;

  // The same as Decrypt(), except that the supplied |nonce| argument rather
  // than the |nonce_| member is used as the nonce. This method is useful
  // for testing the underlying AES GCM implementation.
  QuicData* DecryptWithNonce(base::StringPiece nonce,
                             base::StringPiece associated_data,
                             base::StringPiece ciphertext);

  // The 128-bit AES key.
  unsigned char key_[16];
  // The nonce, a concatenation of a four-byte fixed prefix and a 8-byte
  // packet sequence number.
  unsigned char nonce_[12];
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_AES_128_GCM_DECRYPTER_H_
