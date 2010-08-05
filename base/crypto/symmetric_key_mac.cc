// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/symmetric_key.h"

#include <CommonCrypto/CommonCryptor.h>
#include <CoreFoundation/CFString.h>
#include <Security/cssm.h>

#include "base/crypto/cssm_init.h"
#include "base/logging.h"

namespace {

CSSM_KEY_TYPE CheckKeyParams(base::SymmetricKey::Algorithm algorithm,
                             size_t key_size_in_bits) {
  if (algorithm == base::SymmetricKey::AES) {
    CHECK(key_size_in_bits == 128 ||
        key_size_in_bits == 192 ||
        key_size_in_bits == 256)
        << "Invalid key size " << key_size_in_bits << " bits";
    return CSSM_ALGID_AES;
  } else {
    // FIPS 198 Section 3 requires a HMAC-SHA-1 derived keys to be at least
    // (HMAC-SHA-1 output size / 2) to be compliant. Since the ouput size of
    // HMAC-SHA-1 is 160 bits, we require at least 80 bits here.
    CHECK(algorithm == base::SymmetricKey::HMAC_SHA1);
    CHECK(key_size_in_bits >= 80 && (key_size_in_bits % 8) == 0)
        << "Invalid key size " << key_size_in_bits << " bits";
    return CSSM_ALGID_SHA1HMAC_LEGACY;
  }
}

void* CreateRandomBytes(size_t size) {
  CSSM_RETURN err;
  CSSM_CC_HANDLE ctx;
  err = CSSM_CSP_CreateRandomGenContext(base::GetSharedCSPHandle(),
                                        CSSM_ALGID_APPLE_YARROW,
                                        NULL,
                                        size, &ctx);
  if (err) {
    base::LogCSSMError("CSSM_CSP_CreateRandomGenContext", err);
    return NULL;
  }
  CSSM_DATA random_data = {};
  err = CSSM_GenerateRandom(ctx, &random_data);
  if (err) {
    base::LogCSSMError("CSSM_GenerateRandom", err);
    random_data.Data = NULL;
  }
  CSSM_DeleteContext(ctx);
  return random_data.Data;  // Caller responsible for freeing this
}

inline CSSM_DATA StringToData(const std::string& str) {
  CSSM_DATA data = {
    str.size(),
    reinterpret_cast<uint8_t*>(const_cast<char*>(str.data()))
  };
  return data;
}

}  // namespace

namespace base {

SymmetricKey::~SymmetricKey() {}

// static
SymmetricKey* SymmetricKey::GenerateRandomKey(Algorithm algorithm,
                                              size_t key_size_in_bits) {
  CheckKeyParams(algorithm, key_size_in_bits);
  void* random_bytes = CreateRandomBytes((key_size_in_bits + 7) / 8);
  if (!random_bytes)
    return NULL;
  SymmetricKey *key = new SymmetricKey(random_bytes, key_size_in_bits);
  free(random_bytes);
  return key;
}

// static
SymmetricKey* SymmetricKey::DeriveKeyFromPassword(Algorithm algorithm,
                                                  const std::string& password,
                                                  const std::string& salt,
                                                  size_t iterations,
                                                  size_t key_size_in_bits) {
  // Derived (haha) from cdsaDeriveKey() in Apple's CryptoSample.
  CSSM_KEY_TYPE key_type = CheckKeyParams(algorithm, key_size_in_bits);
  SymmetricKey* derived_key = NULL;
  CSSM_KEY cssm_key = {};

  CSSM_CC_HANDLE ctx = 0;
  CSSM_ACCESS_CREDENTIALS credentials = {};
  CSSM_RETURN err;
  CSSM_DATA salt_data = StringToData(salt);
  err = CSSM_CSP_CreateDeriveKeyContext(GetSharedCSPHandle(),
                                        CSSM_ALGID_PKCS5_PBKDF2,
                                        key_type, key_size_in_bits,
                                        &credentials,
                                        NULL,
                                        iterations,
                                        &salt_data,
                                        NULL,
                                        &ctx);
  if (err) {
    LogCSSMError("CSSM_CSP_CreateDeriveKeyContext", err);
    return NULL;
  }

  CSSM_PKCS5_PBKDF2_PARAMS params = {};
  params.Passphrase = StringToData(password);
  params.PseudoRandomFunction = CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1;
  CSSM_DATA param_data = {sizeof(params), reinterpret_cast<uint8_t*>(&params)};
  err = CSSM_DeriveKey(ctx,
                       &param_data,
                       CSSM_KEYUSE_ANY,
                       CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
                       NULL,
                       NULL,
                       &cssm_key);
  if (err) {
    LogCSSMError("CSSM_DeriveKey", err);
    goto exit;
  }

  DCHECK_EQ(cssm_key.KeyData.Length, key_size_in_bits / 8);
  derived_key = new SymmetricKey(cssm_key.KeyData.Data, key_size_in_bits);

 exit:
  CSSM_DeleteContext(ctx);
  CSSM_FreeKey(GetSharedCSPHandle(), &credentials, &cssm_key, false);
  return derived_key;
}

// static
SymmetricKey* SymmetricKey::Import(Algorithm algorithm,
                                   const std::string& raw_key) {
  return new SymmetricKey(raw_key.data(), raw_key.size() * 8);
}

SymmetricKey::SymmetricKey(const void *key_data, size_t key_size_in_bits)
  : key_(reinterpret_cast<const char*>(key_data),
         key_size_in_bits / 8) {}

bool SymmetricKey::GetRawKey(std::string* raw_key) {
  *raw_key = key_;
  return true;
}

CSSM_DATA SymmetricKey::cssm_data() const {
  return StringToData(key_);
}

}  // namespace base
