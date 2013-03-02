// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic crypto

#ifndef NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
#define NET_QUIC_CRYPTO_CRYPTO_UTILS_H_

#include <string>

#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_protocol.h"

namespace net {

class QuicClock;
class QuicRandom;

class NET_EXPORT_PRIVATE CryptoUtils {
 public:
  // FindMutualTag sets |out_result| to the first tag in |preference| that is
  // also in |supported| and returns true. If there is no intersection between
  // |preference| and |supported| it returns false.
  static bool FindMutualTag(const CryptoTagVector& preference,
                            const CryptoTagVector& supported,
                            CryptoTag* out_result);

  // Generates the connection nonce.
  static void GenerateNonce(const QuicClock* clock,
                            QuicRandom* random_generator,
                            std::string* nonce);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
