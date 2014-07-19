// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/nss/key_nss.h"

#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"

namespace content {

namespace webcrypto {

KeyNss::KeyNss(const CryptoData& serialized_key_data)
    : serialized_key_data_(
          serialized_key_data.bytes(),
          serialized_key_data.bytes() + serialized_key_data.byte_length()) {
}

KeyNss::~KeyNss() {
}

SymKeyNss* KeyNss::AsSymKey() {
  return NULL;
}

PublicKeyNss* KeyNss::AsPublicKey() {
  return NULL;
}

PrivateKeyNss* KeyNss::AsPrivateKey() {
  return NULL;
}

SymKeyNss::~SymKeyNss() {
}

SymKeyNss* SymKeyNss::Cast(const blink::WebCryptoKey& key) {
  KeyNss* platform_key = reinterpret_cast<KeyNss*>(key.handle());
  return platform_key->AsSymKey();
}

SymKeyNss* SymKeyNss::AsSymKey() {
  return this;
}

SymKeyNss::SymKeyNss(crypto::ScopedPK11SymKey key,
                     const CryptoData& raw_key_data)
    : KeyNss(raw_key_data), key_(key.Pass()) {
}

PublicKeyNss::~PublicKeyNss() {
}

PublicKeyNss* PublicKeyNss::Cast(const blink::WebCryptoKey& key) {
  KeyNss* platform_key = reinterpret_cast<KeyNss*>(key.handle());
  return platform_key->AsPublicKey();
}

PublicKeyNss* PublicKeyNss::AsPublicKey() {
  return this;
}

PublicKeyNss::PublicKeyNss(crypto::ScopedSECKEYPublicKey key,
                           const CryptoData& spki_data)
    : KeyNss(spki_data), key_(key.Pass()) {
}

PrivateKeyNss::~PrivateKeyNss() {
}

PrivateKeyNss* PrivateKeyNss::Cast(const blink::WebCryptoKey& key) {
  KeyNss* platform_key = reinterpret_cast<KeyNss*>(key.handle());
  return platform_key->AsPrivateKey();
}

PrivateKeyNss* PrivateKeyNss::AsPrivateKey() {
  return this;
}

PrivateKeyNss::PrivateKeyNss(crypto::ScopedSECKEYPrivateKey key,
                             const CryptoData& pkcs8_data)
    : KeyNss(pkcs8_data), key_(key.Pass()) {
}

bool PlatformSerializeKeyForClone(const blink::WebCryptoKey& key,
                                  blink::WebVector<uint8_t>* key_data) {
  const KeyNss* nss_key = static_cast<KeyNss*>(key.handle());
  *key_data = nss_key->serialized_key_data();
  return true;
}

}  // namespace webcrypto

}  // namespace content
