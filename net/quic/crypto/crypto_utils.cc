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
#include "net/quic/quic_clock.h"

using base::StringPiece;
using std::string;

namespace net {

// static
bool CryptoUtils::FindMutualTag(const CryptoTagVector& our_tags_vector,
                                const CryptoTag* their_tags,
                                size_t num_their_tags,
                                Priority priority,
                                CryptoTag* out_result,
                                size_t* out_index) {
  if (our_tags_vector.empty()) {
    return false;
  }
  const size_t num_our_tags = our_tags_vector.size();
  const CryptoTag* our_tags = &our_tags_vector[0];

  size_t num_priority_tags, num_inferior_tags;
  const CryptoTag* priority_tags;
  const CryptoTag* inferior_tags;
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

void CryptoUtils::GenerateNonce(const QuicClock* clock,
                                QuicRandom* random_generator,
                                const string& orbit,
                                string* nonce) {
  // a 4-byte timestamp + 28 random bytes.
  nonce->reserve(kNonceSize);
  nonce->resize(kNonceSize);
  QuicTime::Delta now = clock->NowAsDeltaSinceUnixEpoch();
  uint32 gmt_unix_time = now.ToSeconds();
  memcpy(&(*nonce)[0], &gmt_unix_time, sizeof(gmt_unix_time));

  size_t bytes_written = sizeof(gmt_unix_time);
  if (orbit.size() == 8) {
    memcpy(&(*nonce)[bytes_written], orbit.data(), orbit.size());
    bytes_written += orbit.size();
  }
  random_generator->RandBytes(&(*nonce)[bytes_written],
                              kNonceSize - bytes_written);
}

void CryptoUtils::DeriveKeys(QuicCryptoNegotiatedParameters* params,
                             StringPiece nonce,
                             const string& hkdf_input,
                             Perspective perspective) {
  params->encrypter.reset(QuicEncrypter::Create(params->aead));
  params->decrypter.reset(QuicDecrypter::Create(params->aead));
  size_t key_bytes = params->encrypter->GetKeySize();
  size_t nonce_prefix_bytes = params->encrypter->GetNoncePrefixSize();

  crypto::HKDF hkdf(params->premaster_secret, nonce,
                    hkdf_input, key_bytes, nonce_prefix_bytes);
  if (perspective == SERVER) {
    params->encrypter->SetKey(hkdf.server_write_key());
    params->encrypter->SetNoncePrefix(hkdf.server_write_iv());
    params->decrypter->SetKey(hkdf.client_write_key());
    params->decrypter->SetNoncePrefix(hkdf.client_write_iv());
  } else {
    params->encrypter->SetKey(hkdf.client_write_key());
    params->encrypter->SetNoncePrefix(hkdf.client_write_iv());
    params->decrypter->SetKey(hkdf.server_write_key());
    params->decrypter->SetNoncePrefix(hkdf.server_write_iv());
  }
}

}  // namespace net
