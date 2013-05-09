// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_utils.h"

#include "crypto/hkdf.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_time.h"

using base::StringPiece;
using std::string;

namespace net {

// static
bool CryptoUtils::FindMutualTag(const QuicTagVector& our_tags_vector,
                                const QuicTag* their_tags,
                                size_t num_their_tags,
                                Priority priority,
                                QuicTag* out_result,
                                size_t* out_index) {
  if (our_tags_vector.empty()) {
    return false;
  }
  const size_t num_our_tags = our_tags_vector.size();
  const QuicTag* our_tags = &our_tags_vector[0];

  size_t num_priority_tags, num_inferior_tags;
  const QuicTag* priority_tags;
  const QuicTag* inferior_tags;
  if (priority == LOCAL_PRIORITY) {
    num_priority_tags = num_our_tags;
    priority_tags = our_tags;
    num_inferior_tags = num_their_tags;
    inferior_tags = their_tags;
  } else {
    num_priority_tags = num_their_tags;
    priority_tags = their_tags;
    num_inferior_tags = num_our_tags;
    inferior_tags = our_tags;
  }

  for (size_t i = 0; i < num_priority_tags; i++) {
    for (size_t j = 0; j < num_inferior_tags; j++) {
      if (priority_tags[i] == inferior_tags[j]) {
        *out_result = priority_tags[i];
        if (out_index) {
          if (priority == LOCAL_PRIORITY) {
            *out_index = j;
          } else {
            *out_index = i;
          }
        }
        return true;
      }
    }
  }

  return false;
}

void CryptoUtils::GenerateNonce(QuicWallTime now,
                                QuicRandom* random_generator,
                                StringPiece orbit,
                                string* nonce) {
  // a 4-byte timestamp + 28 random bytes.
  nonce->reserve(kNonceSize);
  nonce->resize(kNonceSize);
  uint32 gmt_unix_time = now.ToUNIXSeconds();
  // The time in the nonce must be encoded in big-endian because the
  // strike-register depends on the nonces being ordered by time.
  (*nonce)[0] = static_cast<char>(gmt_unix_time >> 24);
  (*nonce)[1] = static_cast<char>(gmt_unix_time >> 16);
  (*nonce)[2] = static_cast<char>(gmt_unix_time >> 8);
  (*nonce)[3] = static_cast<char>(gmt_unix_time);

  size_t bytes_written = sizeof(gmt_unix_time);
  if (orbit.size() == 8) {
    memcpy(&(*nonce)[bytes_written], orbit.data(), orbit.size());
    bytes_written += orbit.size();
  }
  random_generator->RandBytes(&(*nonce)[bytes_written],
                              kNonceSize - bytes_written);
}

void CryptoUtils::DeriveKeys(StringPiece premaster_secret,
                             QuicTag aead,
                             StringPiece client_nonce,
                             StringPiece server_nonce,
                             const string& hkdf_input,
                             Perspective perspective,
                             CrypterPair* out) {
  out->encrypter.reset(QuicEncrypter::Create(aead));
  out->decrypter.reset(QuicDecrypter::Create(aead));
  size_t key_bytes = out->encrypter->GetKeySize();
  size_t nonce_prefix_bytes = out->encrypter->GetNoncePrefixSize();

  StringPiece nonce = client_nonce;
  string nonce_storage;
  if (!server_nonce.empty()) {
    nonce_storage = client_nonce.as_string() + server_nonce.as_string();
    nonce = nonce_storage;
  }

  crypto::HKDF hkdf(premaster_secret, nonce, hkdf_input, key_bytes,
                    nonce_prefix_bytes);
  if (perspective == SERVER) {
    out->encrypter->SetKey(hkdf.server_write_key());
    out->encrypter->SetNoncePrefix(hkdf.server_write_iv());
    out->decrypter->SetKey(hkdf.client_write_key());
    out->decrypter->SetNoncePrefix(hkdf.client_write_iv());
  } else {
    out->encrypter->SetKey(hkdf.client_write_key());
    out->encrypter->SetNoncePrefix(hkdf.client_write_iv());
    out->decrypter->SetKey(hkdf.server_write_key());
    out->decrypter->SetNoncePrefix(hkdf.server_write_iv());
  }
}

}  // namespace net
