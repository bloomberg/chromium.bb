// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic crypto

#ifndef NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
#define NET_QUIC_CRYPTO_CRYPTO_UTILS_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

class QuicClock;
class QuicRandom;
struct CryptoHandshakeMessage;
struct QuicCryptoConfig;
struct QuicCryptoNegotiatedParams;

class NET_EXPORT_PRIVATE CryptoUtils {
 public:
  // Encodes a single value as a string for CryptoTagValueMap.
  template <class T>
  static std::string EncodeSingleValue(const T& value) {
    return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
  }

  // Encodes a vector value as a string for CryptoTagValueMap.
  template <class T>
  static std::string EncodeVectorValue(const std::vector<T>& value) {
    return std::string(reinterpret_cast<const char*>(&value[0]),
                       value.size() * sizeof(value[0]));
  }

  // Generates the connection nonce.
  static void GenerateNonce(const QuicClock* clock,
                            QuicRandom* random_generator,
                            std::string* nonce);

  static void FillClientHelloMessage(const QuicCryptoConfig& client_config,
                                     const std::string& nonce,
                                     const std::string& server_hostname,
                                     CryptoHandshakeMessage* message);

  static void FillServerHelloMessage(
      const QuicCryptoNegotiatedParams& negotiated_params,
      const std::string& nonce,
      CryptoHandshakeMessage* message);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
