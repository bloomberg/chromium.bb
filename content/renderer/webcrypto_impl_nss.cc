// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto_impl.h"

#include <pk11pub.h>
#include <sechash.h>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace {

class SymKeyHandle : public WebKit::WebCryptoKeyHandle {
 public:
  explicit SymKeyHandle(crypto::ScopedPK11SymKey key) {
    DCHECK(!key_.get());
    key_ = key.Pass();
  }

  PK11SymKey* key() { return key_.get(); }

 private:
  crypto::ScopedPK11SymKey key_;

  DISALLOW_COPY_AND_ASSIGN(SymKeyHandle);
};

HASH_HashType WebCryptoAlgorithmToNSSHashType(
    const WebKit::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
      return HASH_AlgSHA1;
    case WebKit::WebCryptoAlgorithmIdSha224:
      return HASH_AlgSHA224;
    case WebKit::WebCryptoAlgorithmIdSha256:
      return HASH_AlgSHA256;
    case WebKit::WebCryptoAlgorithmIdSha384:
      return HASH_AlgSHA384;
    case WebKit::WebCryptoAlgorithmIdSha512:
      return HASH_AlgSHA512;
    default:
      // Not a digest algorithm.
      return HASH_AlgNULL;
  }
}

CK_MECHANISM_TYPE WebCryptoAlgorithmToHMACMechanism(
    const WebKit::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
      return CKM_SHA_1_HMAC;
    case WebKit::WebCryptoAlgorithmIdSha256:
      return CKM_SHA256_HMAC;
    default:
      // Not a supported algorithm.
      return CKM_INVALID_MECHANISM;
  }
}

}  // namespace

void WebCryptoImpl::Init() {
  crypto::EnsureNSSInit();
}

bool WebCryptoImpl::DigestInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  HASH_HashType hash_type = WebCryptoAlgorithmToNSSHashType(algorithm);
  if (hash_type == HASH_AlgNULL) {
    return false;
  }

  HASHContext* context = HASH_Create(hash_type);
  if (!context) {
    return false;
  }

  HASH_Begin(context);

  HASH_Update(context, data, data_size);

  unsigned hash_result_length = HASH_ResultLenContext(context);
  DCHECK_LE(hash_result_length, static_cast<size_t>(HASH_LENGTH_MAX));

  *buffer = WebKit::WebArrayBuffer::create(hash_result_length, 1);

  unsigned char* digest = reinterpret_cast<unsigned char*>(buffer->data());

  unsigned result_length = 0;
  HASH_End(context, digest, &result_length, hash_result_length);

  HASH_Destroy(context);

  return result_length == hash_result_length;
}

bool WebCryptoImpl::ImportKeyInternal(
    WebKit::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const WebKit::WebCryptoAlgorithm& algorithm,
    WebKit::WebCryptoKeyUsageMask usage_mask,
    scoped_ptr<WebKit::WebCryptoKeyHandle>* handle,
    WebKit::WebCryptoKeyType* type) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac:
      *type = WebKit::WebCryptoKeyTypeSecret;
      break;
    // TODO(bryaneyler): Support more key types.
    default:
      return false;
  }

  // TODO(bryaneyler): Need to split handling for symmetric and asymmetric keys.
  // Currently only supporting symmetric.
  CK_MECHANISM_TYPE mechanism = CKM_INVALID_MECHANISM;
  // Flags are verified at the Blink layer; here the flags are set to all
  // possible operations for this key type.
  CK_FLAGS flags = 0;

  switch(algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac: {
      const WebKit::WebCryptoHmacParams* params = algorithm.hmacParams();
      if (!params) {
        return false;
      }

      mechanism = WebCryptoAlgorithmToHMACMechanism(params->hash());
      if (mechanism == CKM_INVALID_MECHANISM) {
        return false;
      }

      flags |= CKF_SIGN | CKF_VERIFY;

      break;
    }
    default:
      return false;
  }

  DCHECK_NE(CKM_INVALID_MECHANISM, mechanism);
  DCHECK_NE(0ul, flags);

  SECItem key_item = { siBuffer, NULL, 0 };

  switch (format) {
    case WebKit::WebCryptoKeyFormatRaw:
      key_item.data = const_cast<unsigned char*>(key_data);
      key_item.len = key_data_size;
      break;
    // TODO(bryaneyler): Handle additional formats.
    default:
      return false;
  }

  crypto::ScopedPK11SymKey pk11_sym_key(
      PK11_ImportSymKeyWithFlags(PK11_GetInternalSlot(),
                                 mechanism,
                                 PK11_OriginUnwrap,
                                 CKA_FLAGS_ONLY,
                                 &key_item,
                                 flags,
                                 false,
                                 NULL));
  if (!pk11_sym_key.get()) {
    NOTREACHED();
    return false;
  }

  scoped_ptr<SymKeyHandle> sym_key(new SymKeyHandle(pk11_sym_key.Pass()));
  *handle = sym_key.Pass();

  return true;
}

bool WebCryptoImpl::SignInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  WebKit::WebArrayBuffer result;

  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac: {
      const WebKit::WebCryptoHmacParams* params = algorithm.hmacParams();
      if (!params) {
        return false;
      }

      SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

      DCHECK_EQ(PK11_GetMechanism(sym_key->key()),
                WebCryptoAlgorithmToHMACMechanism(params->hash()));
      DCHECK_NE(0, key.usages() & WebKit::WebCryptoKeyUsageSign);

      SECItem param_item = { siBuffer, NULL, 0 };
      SECItem data_item = {
        siBuffer,
        const_cast<unsigned char*>(data),
        data_size
      };
      // First call is to figure out the length.
      SECItem signature_item = { siBuffer, NULL, 0 };

      if (PK11_SignWithSymKey(sym_key->key(),
                              PK11_GetMechanism(sym_key->key()),
                              &param_item,
                              &signature_item,
                              &data_item) != SECSuccess) {
        NOTREACHED();
        return false;
      }

      DCHECK_NE(0u, signature_item.len);

      result = WebKit::WebArrayBuffer::create(signature_item.len, 1);
      signature_item.data = reinterpret_cast<unsigned char*>(result.data());

      if (PK11_SignWithSymKey(sym_key->key(),
                              PK11_GetMechanism(sym_key->key()),
                              &param_item,
                              &signature_item,
                              &data_item) != SECSuccess) {
        NOTREACHED();
        return false;
      }

      DCHECK_EQ(result.byteLength(), signature_item.len);

      break;
    }
    default:
      return false;
  }

  *buffer = result;
  return true;
}

}  // namespace content
