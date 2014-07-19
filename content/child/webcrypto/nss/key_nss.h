// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_NSS_KEY_NSS_H_
#define CONTENT_CHILD_WEBCRYPTO_NSS_KEY_NSS_H_

#include <stdint.h>
#include <vector>

#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace webcrypto {

class CryptoData;
class PrivateKeyNss;
class PublicKeyNss;
class SymKeyNss;

// Base key class for all NSS keys, used to safely cast between types. Each key
// maintains a copy of its serialized form in either 'raw', 'pkcs8', or 'spki'
// format. This is to allow structured cloning of keys synchronously from the
// target Blink thread without having to lock access to the key.
class KeyNss : public blink::WebCryptoKeyHandle {
 public:
  explicit KeyNss(const CryptoData& serialized_key_data);
  virtual ~KeyNss();

  virtual SymKeyNss* AsSymKey();
  virtual PublicKeyNss* AsPublicKey();
  virtual PrivateKeyNss* AsPrivateKey();

  const std::vector<uint8_t>& serialized_key_data() const {
    return serialized_key_data_;
  }

 private:
  const std::vector<uint8_t> serialized_key_data_;
};

class SymKeyNss : public KeyNss {
 public:
  virtual ~SymKeyNss();
  SymKeyNss(crypto::ScopedPK11SymKey key, const CryptoData& raw_key_data);

  static SymKeyNss* Cast(const blink::WebCryptoKey& key);

  PK11SymKey* key() { return key_.get(); }
  virtual SymKeyNss* AsSymKey() OVERRIDE;

  const std::vector<uint8_t>& raw_key_data() const {
    return serialized_key_data();
  }

 private:
  crypto::ScopedPK11SymKey key_;

  DISALLOW_COPY_AND_ASSIGN(SymKeyNss);
};

class PublicKeyNss : public KeyNss {
 public:
  virtual ~PublicKeyNss();
  PublicKeyNss(crypto::ScopedSECKEYPublicKey key, const CryptoData& spki_data);

  static PublicKeyNss* Cast(const blink::WebCryptoKey& key);

  SECKEYPublicKey* key() { return key_.get(); }
  virtual PublicKeyNss* AsPublicKey() OVERRIDE;

  const std::vector<uint8_t>& spki_data() const {
    return serialized_key_data();
  }

 private:
  crypto::ScopedSECKEYPublicKey key_;

  DISALLOW_COPY_AND_ASSIGN(PublicKeyNss);
};

class PrivateKeyNss : public KeyNss {
 public:
  virtual ~PrivateKeyNss();
  PrivateKeyNss(crypto::ScopedSECKEYPrivateKey key,
                const CryptoData& pkcs8_data);

  static PrivateKeyNss* Cast(const blink::WebCryptoKey& key);

  SECKEYPrivateKey* key() { return key_.get(); }
  virtual PrivateKeyNss* AsPrivateKey() OVERRIDE;

  const std::vector<uint8_t>& pkcs8_data() const {
    return serialized_key_data();
  }

 private:
  crypto::ScopedSECKEYPrivateKey key_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKeyNss);
};

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_NSS_KEY_NSS_H_
