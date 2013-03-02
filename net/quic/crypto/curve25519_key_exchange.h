// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CURVE25519_KEY_EXCHANGE_H_
#define NET_QUIC_CRYPTO_CURVE25519_KEY_EXCHANGE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/key_exchange.h"

namespace net {

class QuicRandom;

// Curve25519KeyExchange implements a KeyExchange using elliptic-curve
// Diffie-Hellman on curve25519. See http://cr.yp.to/ecdh.html
class NET_EXPORT_PRIVATE Curve25519KeyExchange : public KeyExchange {
 public:
  virtual ~Curve25519KeyExchange();

  // New creates a new object from a private key. If the private key is
  // invalid, NULL is returned.
  static Curve25519KeyExchange* New(const base::StringPiece& private_key);

  // NewPrivateKey returns a private key, generated from |rand|, suitable for
  // passing to |New|.
  static std::string NewPrivateKey(QuicRandom* rand);

  // KeyExchange interface.
  virtual bool CalculateSharedKey(const base::StringPiece& their_public_value,
                                  std::string* shared_key) const OVERRIDE;
  virtual base::StringPiece public_value() const OVERRIDE;
  virtual CryptoTag tag() const OVERRIDE;

 private:
  Curve25519KeyExchange();

// TODO(rtenneti): Add support for curve25519.
#if 0
  uint8 private_key_[32];
#endif
  uint8 public_key_[32];
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CURVE25519_KEY_EXCHANGE_H_
