// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_KEY_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_KEY_OPENSSL_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace webcrypto {

class CryptoData;
class SymKeyOpenSsl;

// Base key class for all OpenSSL keys, used to safely cast between types. Each
// key maintains a copy of its serialized form in either 'raw', 'pkcs8', or
// 'spki' format. This is to allow structured cloning of keys synchronously from
// the target Blink thread without having to lock access to the key.
class KeyOpenSsl : public blink::WebCryptoKeyHandle {
 public:
  explicit KeyOpenSsl(const CryptoData& serialized_key_data);
  virtual ~KeyOpenSsl();

  virtual SymKeyOpenSsl* AsSymKey();

  const std::vector<uint8_t>& serialized_key_data() const {
    return serialized_key_data_;
  }

 private:
  const std::vector<uint8_t> serialized_key_data_;
};

class SymKeyOpenSsl : public KeyOpenSsl {
 public:
  virtual ~SymKeyOpenSsl();
  explicit SymKeyOpenSsl(const CryptoData& raw_key_data);

  static SymKeyOpenSsl* Cast(const blink::WebCryptoKey& key);

  virtual SymKeyOpenSsl* AsSymKey() OVERRIDE;

  const std::vector<uint8_t>& raw_key_data() const {
    return serialized_key_data();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SymKeyOpenSsl);
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_KEY_OPENSSL_H_
