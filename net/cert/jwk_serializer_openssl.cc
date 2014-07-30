// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/jwk_serializer.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace net {

namespace JwkSerializer {

namespace {

bool ConvertEcKeyToJwk(EVP_PKEY* pkey,
                       base::DictionaryValue* public_key_jwk,
                       const crypto::OpenSSLErrStackTracer& err_tracer) {
  crypto::ScopedEC_KEY ec_key(EVP_PKEY_get1_EC_KEY(pkey));
  if (!ec_key)
    return false;
  const EC_GROUP* ec_group = EC_KEY_get0_group(ec_key.get());
  if (!ec_group)
    return false;

  std::string curve_name;
  int nid = EC_GROUP_get_curve_name(ec_group);
  if (nid == NID_X9_62_prime256v1) {
    curve_name = "P-256";
  } else if (nid == NID_secp384r1) {
    curve_name = "P-384";
  } else if (nid == NID_secp521r1) {
    curve_name = "P-521";
  } else {
    return false;
  }

  int degree_bytes = (EC_GROUP_get_degree(ec_group) + 7) / 8;

  const EC_POINT* ec_point = EC_KEY_get0_public_key(ec_key.get());
  if (!ec_point)
    return false;

  crypto::ScopedBIGNUM x(BN_new());
  crypto::ScopedBIGNUM y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(ec_group, ec_point,
                                           x.get(), y.get(), NULL)) {
    return false;
  }

  // The coordinates are encoded with leading zeros included.
  std::string x_bytes;
  std::string y_bytes;
  if (!BN_bn2bin_padded(reinterpret_cast<uint8_t*>(
          WriteInto(&x_bytes, degree_bytes + 1)), degree_bytes, x.get()) ||
      !BN_bn2bin_padded(reinterpret_cast<uint8_t*>(
          WriteInto(&y_bytes, degree_bytes + 1)), degree_bytes, y.get())) {
    return false;
  }

  public_key_jwk->SetString("kty", "EC");
  public_key_jwk->SetString("crv", curve_name);

  std::string x_b64;
  base::Base64Encode(x_bytes, &x_b64);
  public_key_jwk->SetString("x", x_b64);

  std::string y_b64;
  base::Base64Encode(y_bytes, &y_b64);
  public_key_jwk->SetString("y", y_b64);

  return true;
}

}  // namespace

bool ConvertSpkiFromDerToJwk(
    const base::StringPiece& spki_der,
    base::DictionaryValue* public_key_jwk) {
  public_key_jwk->Clear();

  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const uint8_t *data = reinterpret_cast<const uint8_t*>(spki_der.data());
  const uint8_t *ptr = data;
  crypto::ScopedEVP_PKEY pubkey(d2i_PUBKEY(NULL, &ptr, spki_der.size()));
  if (!pubkey || ptr != data + spki_der.size())
    return false;

  if (pubkey->type == EVP_PKEY_EC) {
    return ConvertEcKeyToJwk(pubkey.get(), public_key_jwk, err_tracer);
  } else {
    // TODO(juanlang): other algorithms
    return false;
  }
}

}  // namespace JwkSerializer

}  // namespace net
