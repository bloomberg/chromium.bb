// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/openssl/key_openssl.h"

#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "components/webcrypto/webcrypto_util.h"

namespace webcrypto {

KeyOpenSsl::KeyOpenSsl(const CryptoData& serialized_key_data)
    : serialized_key_data_(
          serialized_key_data.bytes(),
          serialized_key_data.bytes() + serialized_key_data.byte_length()) {
}

KeyOpenSsl::~KeyOpenSsl() {
}

SymKeyOpenSsl* KeyOpenSsl::AsSymKey() {
  return NULL;
}

AsymKeyOpenSsl* KeyOpenSsl::AsAsymKey() {
  return NULL;
}

SymKeyOpenSsl::~SymKeyOpenSsl() {
}

SymKeyOpenSsl* SymKeyOpenSsl::Cast(const blink::WebCryptoKey& key) {
  KeyOpenSsl* platform_key = reinterpret_cast<KeyOpenSsl*>(key.handle());
  return platform_key->AsSymKey();
}

SymKeyOpenSsl* SymKeyOpenSsl::AsSymKey() {
  return this;
}

SymKeyOpenSsl::SymKeyOpenSsl(const CryptoData& raw_key_data)
    : KeyOpenSsl(raw_key_data) {
}

AsymKeyOpenSsl::~AsymKeyOpenSsl() {
}

AsymKeyOpenSsl* AsymKeyOpenSsl::Cast(const blink::WebCryptoKey& key) {
  return reinterpret_cast<KeyOpenSsl*>(key.handle())->AsAsymKey();
}

AsymKeyOpenSsl* AsymKeyOpenSsl::AsAsymKey() {
  return this;
}

AsymKeyOpenSsl::AsymKeyOpenSsl(crypto::ScopedEVP_PKEY key,
                               const CryptoData& serialized_key_data)
    : KeyOpenSsl(serialized_key_data), key_(key.Pass()) {
}

}  // namespace webcrypto
