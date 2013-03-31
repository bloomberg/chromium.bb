// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/aes_128_gcm_decrypter.h"

#include <pk11pub.h>

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
bool Aes128GcmDecrypter::IsSupported() {
#if defined(USE_NSS)
  // We're using system NSS libraries. Regrettably NSS 3.14.x has a bug in the
  // AES GCM code (NSS bug 853285) and is missing the PK11_Decrypt function
  // (NSS bug 854063).  Both problems should be fixed in NSS 3.15.
  return false;
#else
  // We're using our own copy of NSS.
  return true;
#endif
}

bool Aes128GcmDecrypter::SetKey(StringPiece key) {
  DCHECK_EQ(key.size(), sizeof(key_));
  if (key.size() != sizeof(key_)) {
    return false;
  }
  memcpy(key_, key.data(), key.size());
  return true;
}

bool Aes128GcmDecrypter::SetNoncePrefix(StringPiece nonce_prefix) {
  DCHECK_EQ(nonce_prefix.size(), kNoncePrefixSize);
  if (nonce_prefix.size() != kNoncePrefixSize) {
    return false;
  }
  memcpy(nonce_, nonce_prefix.data(), nonce_prefix.size());
  return true;
}

bool Aes128GcmDecrypter::Decrypt(StringPiece nonce,
                                 StringPiece associated_data,
                                 StringPiece ciphertext,
                                 unsigned char* output,
                                 size_t* output_length) {
#if defined(USE_NSS)
  // See the comment in IsSupported().
  return false;
#else
  if (ciphertext.length() < kAuthTagSize ||
      nonce.size() != kNoncePrefixSize + sizeof(QuicPacketSequenceNumber)) {
    return false;
  }
  // NSS 3.14.x incorrectly requires an output buffer at least as long as
  // the ciphertext (NSS bug 853674). Fortunately QuicDecrypter::Decrypt()
  // specifies that |output| must be as long as |ciphertext| on entry.
  size_t plaintext_size = ciphertext.length() - kAuthTagSize;

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
      slot, CKM_AES_GCM, PK11_OriginUnwrap, CKA_DECRYPT, &key_item, NULL));
  PK11_FreeSlot(slot);
  slot = NULL;
  if (!aes_key) {
    DLOG(INFO) << "PK11_ImportSymKey failed";
    return false;
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
  // If an incorrect authentication tag causes a decryption failure, the NSS
  // error is SEC_ERROR_BAD_DATA (-8190).
  if (PK11_Decrypt(aes_key.get(), CKM_AES_GCM, &param,
                   output, &output_len, ciphertext.length(),
                   reinterpret_cast<const unsigned char*>(
                   ciphertext.data()), ciphertext.length()) != SECSuccess) {
    DLOG(INFO) << "PK11_Decrypt failed: NSS error " << PORT_GetError();
    return false;
  }

  if (output_len != plaintext_size) {
    DLOG(INFO) << "Wrong output length";
    return false;
  }
  *output_length = output_len;
  return true;
#endif
}

QuicData* Aes128GcmDecrypter::DecryptPacket(
    QuicPacketSequenceNumber sequence_number,
    StringPiece associated_data,
    StringPiece ciphertext) {
  COMPILE_ASSERT(sizeof(nonce_) == kNoncePrefixSize + sizeof(sequence_number),
                 incorrect_nonce_size);
  if (ciphertext.length() < kAuthTagSize) {
    return NULL;
  }
  size_t plaintext_size;
  scoped_ptr<char[]> plaintext(new char[ciphertext.length()]);

  memcpy(nonce_ + kNoncePrefixSize, &sequence_number, sizeof(sequence_number));
  if (!Decrypt(StringPiece(reinterpret_cast<char*>(nonce_), sizeof(nonce_)),
               associated_data, ciphertext,
               reinterpret_cast<unsigned char*>(plaintext.get()),
               &plaintext_size)) {
    return NULL;
  }
  return new QuicData(plaintext.release(), plaintext_size, true);
}

StringPiece Aes128GcmDecrypter::GetKey() const {
  return StringPiece(reinterpret_cast<const char*>(key_), sizeof(key_));
}

StringPiece Aes128GcmDecrypter::GetNoncePrefix() const {
  return StringPiece(reinterpret_cast<const char*>(nonce_), kNoncePrefixSize);
}

}  // namespace net
