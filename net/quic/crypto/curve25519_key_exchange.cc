// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/curve25519_key_exchange.h"

#include <string.h>

#include "base/logging.h"
#include "net/quic/crypto/quic_random.h"

// TODO(rtenneti): Remove the following line after support for curve25519 is
// added.
#define crypto_scalarmult_curve25519_SCALARBYTES 32

using base::StringPiece;
using std::string;

namespace net {

Curve25519KeyExchange::Curve25519KeyExchange() {
}

Curve25519KeyExchange::~Curve25519KeyExchange() {
}

// static
Curve25519KeyExchange* Curve25519KeyExchange::New(
    const StringPiece& private_key) {
// TODO(rtenneti): Add support for curve25519.
#if 0
  crypto_scalarmult_curve25519_base(ka->public_key_, ka->private_key_);
  Curve25519KeyExchange* ka;

  // We don't want to #include the NaCl headers in the public header file, so
  // we use literals for the sizes of private_key_ and public_key_. Here we
  // assert that those values are equal to the values from the NaCl header.
  COMPILE_ASSERT(
      sizeof(ka->private_key_) == crypto_scalarmult_curve25519_SCALARBYTES,
      header_out_of_sync);
  COMPILE_ASSERT(
      sizeof(ka->public_key_) == crypto_scalarmult_curve25519_BYTES,
      header_out_of_sync);

  if (private_key.size() != crypto_scalarmult_curve25519_SCALARBYTES) {
    return NULL;
  }

  ka = new Curve25519KeyExchange();
  memcpy(ka->private_key_, private_key.data(),
         crypto_scalarmult_curve25519_SCALARBYTES);
  return ka;
#else
  return NULL;
#endif
}

// static
string Curve25519KeyExchange::NewPrivateKey(QuicRandom* rand) {
  uint8 private_key[crypto_scalarmult_curve25519_SCALARBYTES];
  rand->RandBytes(private_key, sizeof(private_key));

  // This makes |private_key| a valid scalar, as specified on
  // http://cr.yp.to/ecdh.html
  private_key[0] &= 248;
  private_key[31] &= 127;
  private_key[31] |= 64;
  return string(reinterpret_cast<char*>(private_key), sizeof(private_key));
}

bool Curve25519KeyExchange::CalculateSharedKey(
    const StringPiece& public_value,
    string* out_result) const {
// TODO(rtenneti): Add support for curve25519.
#if 0
  if (public_value.size() != crypto_scalarmult_curve25519_BYTES) {
    return false;
  }

  uint8 result[crypto_scalarmult_curve25519_BYTES];
  crypto_scalarmult_curve25519(
      result, private_key_,
      reinterpret_cast<const uint8*>(public_value.data()));
  out_result->assign(reinterpret_cast<char*>(result), sizeof(result));

  return true;
#else
  return false;
#endif
}

StringPiece Curve25519KeyExchange::public_value() const {
  return StringPiece(reinterpret_cast<const char*>(public_key_),
                     sizeof(public_key_));
}

CryptoTag Curve25519KeyExchange::tag() const {
  return kC255;
}

}  // namespace net
