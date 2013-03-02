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
bool CryptoUtils::FindMutualTag(const CryptoTagVector& preference,
                                const CryptoTagVector& supported,
                                CryptoTag* out_result) {
  for (CryptoTagVector::const_iterator i = preference.begin();
       i != preference.end(); i++) {
    for (CryptoTagVector::const_iterator j = supported.begin();
         j != supported.end(); j++) {
      if (*i == *j) {
        *out_result = *i;
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
