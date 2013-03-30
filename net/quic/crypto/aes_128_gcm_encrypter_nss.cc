// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_encrypter.h"

#include <pk11pub.h>
#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "crypto/scoped_nss_types.h"

using base::StringPiece;

namespace net {

namespace {

const size_t kKeySize = 16;
const size_t kNoncePrefixSize = 4;
const size_t kAuthTagSize = 16;

}  // namespace

// static
bool Aes128GcmEncrypter::IsSupported() {
#if defined(USE_NSS)
  // We're using system NSS libraries. Regrettably NSS 3.14.x has a bug in the
  // AES GCM code (NSS bug 853285) and is missing the PK11_Encrypt function
  // (NSS bug 854063).  Both problems should be fixed in NSS 3.15.
  return false;
#else
  // We're using our own copy of NSS.
  return true;
#endif
}

bool Aes128GcmEncrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());
  return true;
}

bool Aes128GcmEncrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  memcpy(nonce_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

QuicData* Aes128GcmEncrypter::Encrypt(QuicPacketSequenceNumber sequence_number,
                                      StringPiece associated_data,
                                      StringPiece plaintext) {
  COMPILE_ASSERT(sizeof(nonce_) == kNoncePrefixSize + sizeof(sequence_number),
                 incorrect_nonce_size);
  memcpy(nonce_ + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));
  return EncryptWithNonce(StringPiece(reinterpret_cast<char*>(nonce_),
                                      sizeof(nonce_)),
                          associated_data, plaintext);
}

size_t Aes128GcmEncrypter::GetKeySize() const {
  return kKeySize;
}

size_t Aes128GcmEncrypter::GetNoncePrefixSize() const {
  return kNoncePrefixSize;
}

size_t Aes128GcmEncrypter::GetMaxPlaintextSize(size_t ciphertext_size) const {
  return ciphertext_size - kAuthTagSize;
}

// An AEAD_AES_128_GCM ciphertext is exactly 16 bytes longer than its
// corresponding plaintext.
size_t Aes128GcmEncrypter::GetCiphertextSize(size_t plaintext_size) const {
  return plaintext_size + kAuthTagSize;
}

StringPiece Aes128GcmEncrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), sizeof(key_));
}

StringPiece Aes128GcmEncrypter::GetNoncePrefix() const {
  return StringPiece(reinterpret_cast<const char*>(nonce_), kNoncePrefixSize);
}

QuicData* Aes128GcmEncrypter::EncryptWithNonce(StringPiece nonce,
                                               StringPiece associated_data,
                                               StringPiece plaintext) {
#if defined(USE_NSS)
  // See the comment in IsSupported().
  return NULL;
#else
  size_t ciphertext_size = GetCiphertextSize(plaintext.length());
  scoped_ptr<char[]> ciphertext(new char[ciphertext_size]);

  // Import key_ into NSS.
  SECItem key_item;
  key_item.type = siBuffer;
  key_item.data = key_;
  key_item.len = sizeof(key_);
  PK11SlotInfo* slot = PK11_GetInternalSlot();
  // The exact value of the |origin| argument doesn't matter to NSS as long as
  // it's not PK11_OriginFortezzaHack, so we pass PK11_OriginUnwrap as a
  // placeholder.
  crypto::ScopedPK11SymKey aes_key(PK11_ImportSymKey(
      slot, CKM_AES_GCM, PK11_OriginUnwrap, CKA_ENCRYPT, &key_item, NULL));
  PK11_FreeSlot(slot);
  slot = NULL;
  if (!aes_key) {
    DLOG(INFO) << "PK11_ImportSymKey failed";
    return NULL;
  }

  CK_GCM_PARAMS gcm_params;
  gcm_params.pIv =
      reinterpret_cast<CK_BYTE*>(const_cast<char*>(nonce.data()));
  gcm_params.ulIvLen = nonce.size();
  gcm_params.pAAD =
      reinterpret_cast<CK_BYTE*>(const_cast<char*>(associated_data.data()));
  gcm_params.ulAADLen = associated_data.size();
  gcm_params.ulTagBits = kAuthTagSize * 8;

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&gcm_params);
  param.len = sizeof(gcm_params);

  unsigned int output_len;
  if (PK11_Encrypt(aes_key.get(), CKM_AES_GCM, &param,
                   reinterpret_cast<unsigned char*>(ciphertext.get()),
                   &output_len, ciphertext_size,
                   reinterpret_cast<const unsigned char*>(plaintext.data()),
                   plaintext.size()) != SECSuccess) {
    DLOG(INFO) << "PK11_Encrypt failed";
    return NULL;
  }

  if (output_len != ciphertext_size) {
    DLOG(INFO) << "Wrong output length";
    return NULL;
  }

  return new QuicData(ciphertext.release(), ciphertext_size, true);
#endif
}

}  // namespace net
