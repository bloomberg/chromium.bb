// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some helpers for quic crypto

#ifndef NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
#define NET_QUIC_CRYPTO_CRYPTO_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_protocol.h"

namespace net {

class QuicTime;
class QuicRandom;
struct QuicCryptoNegotiatedParameters;

class NET_EXPORT_PRIVATE CryptoUtils {
 public:
  enum Priority {
    LOCAL_PRIORITY,
    PEER_PRIORITY,
  };

  enum Perspective {
    SERVER,
    CLIENT,
  };

  // FindMutualTag sets |out_result| to the first tag in the priority list that
  // is also in the other list and returns true. If there is no intersection it
  // returns false.
  //
  // Which list has priority is determined by |priority|.
  //
  // If |out_index| is non-NULL and a match is found then the index of that
  // match in |their_tags| is written to |out_index|.
  static bool FindMutualTag(const CryptoTagVector& our_tags,
                            const CryptoTag* their_tags,
                            size_t num_their_tags,
                            Priority priority,
                            CryptoTag* out_result,
                            size_t* out_index);

  // Generates the connection nonce. The nonce is formed as:
  //   <4 bytes> current time
  //   <8 bytes> |orbit| (or random if |orbit| is empty)
  //   <20 bytes> random
  static void GenerateNonce(QuicTime::Delta now,
                            QuicRandom* random_generator,
                            base::StringPiece orbit,
                            std::string* nonce);

  // DeriveKeys populates |params->encrypter| and |params->decrypter| given the
  // contents of |params->premaster_secret|, |client_nonce|,
  // |params->server_nonce| and |hkdf_input|. |perspective| controls whether
  // the server's keys are assigned to |encrypter| or |decrypter|.
  // |params->server_nonce| is optional and, if non-empty, is mixed into the
  // key derivation.
  static void DeriveKeys(QuicCryptoNegotiatedParameters* params,
                         base::StringPiece client_nonce,
                         const std::string& hkdf_input,
                         Perspective perspective);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_UTILS_H_
