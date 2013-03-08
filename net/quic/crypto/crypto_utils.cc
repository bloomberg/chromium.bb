// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_utils.h"

#include "base/string_piece.h"
#include "net/quic/crypto/crypto_protocol.h"
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
                                string* nonce) {
  // a 4-byte timestamp + 28 random bytes.
  nonce->reserve(kNonceSize);
  nonce->resize(kNonceSize);
  QuicTime::Delta now = clock->NowAsDeltaSinceUnixEpoch();
  uint32 gmt_unix_time = now.ToSeconds();
  const size_t time_size = sizeof(gmt_unix_time);
  memcpy(&(*nonce)[0], &gmt_unix_time, time_size);
  random_generator->RandBytes(&(*nonce)[time_size], kNonceSize - time_size);
}

}  // namespace net
