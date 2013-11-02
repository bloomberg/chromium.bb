// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <cryptohi.h>
#include <pk11pub.h>
#include <sechash.h>

#include <vector>

#include "base/logging.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace {

class SymKeyHandle : public WebKit::WebCryptoKeyHandle {
 public:
  explicit SymKeyHandle(crypto::ScopedPK11SymKey key) : key_(key.Pass()) {}

  PK11SymKey* key() { return key_.get(); }

 private:
  crypto::ScopedPK11SymKey key_;

  DISALLOW_COPY_AND_ASSIGN(SymKeyHandle);
};

class PublicKeyHandle : public WebKit::WebCryptoKeyHandle {
 public:
  explicit PublicKeyHandle(crypto::ScopedSECKEYPublicKey key)
      : key_(key.Pass()) {}

  SECKEYPublicKey* key() { return key_.get(); }

 private:
  crypto::ScopedSECKEYPublicKey key_;

  DISALLOW_COPY_AND_ASSIGN(PublicKeyHandle);
};

class PrivateKeyHandle : public WebKit::WebCryptoKeyHandle {
 public:
  explicit PrivateKeyHandle(crypto::ScopedSECKEYPrivateKey key)
      : key_(key.Pass()) {}

  SECKEYPrivateKey* key() { return key_.get(); }

 private:
  crypto::ScopedSECKEYPrivateKey key_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKeyHandle);
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

bool AesCbcEncryptDecrypt(
    CK_ATTRIBUTE_TYPE operation,
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  DCHECK_EQ(WebKit::WebCryptoAlgorithmIdAesCbc, algorithm.id());
  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK_EQ(WebKit::WebCryptoKeyTypeSecret, key.type());
  DCHECK(operation == CKA_ENCRYPT || operation == CKA_DECRYPT);

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  const WebKit::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
  if (params->iv().size() != AES_BLOCK_SIZE)
    return false;

  SECItem iv_item;
  iv_item.type = siBuffer;
  iv_item.data = const_cast<unsigned char*>(params->iv().data());
  iv_item.len = params->iv().size();

  crypto::ScopedSECItem param(PK11_ParamFromIV(CKM_AES_CBC_PAD, &iv_item));
  if (!param)
    return false;

  crypto::ScopedPK11Context context(PK11_CreateContextBySymKey(
      CKM_AES_CBC_PAD, operation, sym_key->key(), param.get()));

  if (!context.get())
    return false;

  // Oddly PK11_CipherOp takes input and output lengths as "int" rather than
  // "unsigned". Do some checks now to avoid integer overflowing.
  if (data_size >= INT_MAX - AES_BLOCK_SIZE) {
    // TODO(eroman): Handle this by chunking the input fed into NSS. Right now
    // it doesn't make much difference since the one-shot API would end up
    // blowing out the memory and crashing anyway. However a newer version of
    // the spec allows for a sequence<CryptoData> so this will be relevant.
    return false;
  }

  // PK11_CipherOp does an invalid memory access when given empty decryption
  // input, or input which is not a multiple of the block size. See also
  // https://bugzilla.mozilla.com/show_bug.cgi?id=921687.
  if (operation == CKA_DECRYPT &&
      (data_size == 0 || (data_size % AES_BLOCK_SIZE != 0))) {
    return false;
  }

  // TODO(eroman): Refine the output buffer size. It can be computed exactly for
  //               encryption, and can be smaller for decryption.
  unsigned output_max_len = data_size + AES_BLOCK_SIZE;
  CHECK_GT(output_max_len, data_size);

  *buffer = WebKit::WebArrayBuffer::create(output_max_len, 1);

  unsigned char* buffer_data = reinterpret_cast<unsigned char*>(buffer->data());

  int output_len;
  if (SECSuccess != PK11_CipherOp(context.get(),
                                  buffer_data,
                                  &output_len,
                                  buffer->byteLength(),
                                  data,
                                  data_size)) {
    return false;
  }

  unsigned int final_output_chunk_len;
  if (SECSuccess != PK11_DigestFinal(context.get(),
                                     buffer_data + output_len,
                                     &final_output_chunk_len,
                                     output_max_len - output_len)) {
    return false;
  }

  WebCryptoImpl::ShrinkBuffer(buffer, final_output_chunk_len + output_len);
  return true;
}

CK_MECHANISM_TYPE HmacAlgorithmToGenMechanism(
    const WebKit::WebCryptoAlgorithm& algorithm) {
  DCHECK_EQ(algorithm.id(), WebKit::WebCryptoAlgorithmIdHmac);
  const WebKit::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
  DCHECK(params);
  switch (params->hash().id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
      return CKM_SHA_1_HMAC;
    case WebKit::WebCryptoAlgorithmIdSha256:
      return CKM_SHA256_HMAC;
    default:
      return CKM_INVALID_MECHANISM;
  }
}

CK_MECHANISM_TYPE WebCryptoAlgorithmToGenMechanism(
    const WebKit::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdAesCbc:
      return CKM_AES_KEY_GEN;
    case WebKit::WebCryptoAlgorithmIdHmac:
      return HmacAlgorithmToGenMechanism(algorithm);
    default:
      return CKM_INVALID_MECHANISM;
  }
}

unsigned int WebCryptoHmacAlgorithmToBlockSize(
    const WebKit::WebCryptoAlgorithm& algorithm) {
  DCHECK_EQ(algorithm.id(), WebKit::WebCryptoAlgorithmIdHmac);
  const WebKit::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
  DCHECK(params);
  switch (params->hash().id()) {
    case WebKit::WebCryptoAlgorithmIdSha1:
      return 512;
    case WebKit::WebCryptoAlgorithmIdSha256:
      return 512;
    default:
      return 0;
  }
}

// Converts a (big-endian) WebCrypto BigInteger, with or without leading zeros,
// to unsigned long.
bool BigIntegerToLong(const uint8* data,
                      unsigned data_size,
                      unsigned long* result) {
  // TODO(padolph): Is it correct to say that empty data is an error, or does it
  // mean value 0? See https://www.w3.org/Bugs/Public/show_bug.cgi?id=23655
  if (data_size == 0)
    return false;

  *result = 0;
  for (size_t i = 0; i < data_size; ++i) {
    size_t reverse_i = data_size - i - 1;

    if (reverse_i >= sizeof(unsigned long) && data[i])
      return false;  // Too large for a long.

    *result |= data[i] << 8 * reverse_i;
  }
  return true;
}

}  // namespace

void WebCryptoImpl::Init() {
  crypto::EnsureNSSInit();
}

bool WebCryptoImpl::EncryptInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  if (algorithm.id() == WebKit::WebCryptoAlgorithmIdAesCbc) {
    return AesCbcEncryptDecrypt(
        CKA_ENCRYPT, algorithm, key, data, data_size, buffer);
  }

  return false;
}

bool WebCryptoImpl::DecryptInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  if (algorithm.id() == WebKit::WebCryptoAlgorithmIdAesCbc) {
    return AesCbcEncryptDecrypt(
        CKA_DECRYPT, algorithm, key, data, data_size, buffer);
  }

  return false;
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

bool WebCryptoImpl::GenerateKeyInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    bool extractable,
    WebKit::WebCryptoKeyUsageMask usage_mask,
    WebKit::WebCryptoKey* key) {

  CK_MECHANISM_TYPE mech = WebCryptoAlgorithmToGenMechanism(algorithm);
  unsigned int keylen_bytes = 0;
  WebKit::WebCryptoKeyType key_type = WebKit::WebCryptoKeyTypeSecret;

  if (mech == CKM_INVALID_MECHANISM) {
    return false;
  }

  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdAesCbc: {
      const WebKit::WebCryptoAesKeyGenParams* params =
          algorithm.aesKeyGenParams();
      DCHECK(params);
      keylen_bytes = params->length() / 8;
      if (params->length() % 8)
        return false;
      key_type = WebKit::WebCryptoKeyTypeSecret;
      break;
    }
    case WebKit::WebCryptoAlgorithmIdHmac: {
      const WebKit::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
      DCHECK(params);
      if (!params->getLength(keylen_bytes)) {
        keylen_bytes = WebCryptoHmacAlgorithmToBlockSize(algorithm) / 8;
      }

      key_type = WebKit::WebCryptoKeyTypeSecret;
      break;
    }

    default: {
      return false;
    }
  }

  if (keylen_bytes == 0) {
    return false;
  }

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot) {
    return false;
  }

  crypto::ScopedPK11SymKey pk11_key(
      PK11_KeyGen(slot.get(), mech, NULL, keylen_bytes, NULL));

  if (!pk11_key) {
    return false;
  }

  *key = WebKit::WebCryptoKey::create(
      new SymKeyHandle(pk11_key.Pass()),
      key_type, extractable, algorithm, usage_mask);
  return true;
}

bool WebCryptoImpl::GenerateKeyPairInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    bool extractable,
    WebKit::WebCryptoKeyUsageMask usage_mask,
    WebKit::WebCryptoKey* public_key,
    WebKit::WebCryptoKey* private_key) {

  // TODO(padolph): Handle other asymmetric algorithm key generation.
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
    case WebKit::WebCryptoAlgorithmIdRsaOaep:
    case WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5: {
      const WebKit::WebCryptoRsaKeyGenParams* const params =
          algorithm.rsaKeyGenParams();
      DCHECK(params);

      crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
      unsigned long public_exponent;
      if (!slot || !params->modulusLength() ||
          !BigIntegerToLong(params->publicExponent().data(),
                            params->publicExponent().size(),
                            &public_exponent) ||
          !public_exponent) {
        return false;
      }

      PK11RSAGenParams rsa_gen_params;
      rsa_gen_params.keySizeInBits = params->modulusLength();
      rsa_gen_params.pe = public_exponent;

      // Flags are verified at the Blink layer; here the flags are set to all
      // possible operations for the given key type.
      CK_FLAGS operation_flags;
      switch (algorithm.id()) {
        case WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
        case WebKit::WebCryptoAlgorithmIdRsaOaep:
          operation_flags = CKF_ENCRYPT | CKF_DECRYPT | CKF_WRAP | CKF_UNWRAP;
          break;
        case WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
          operation_flags = CKF_SIGN | CKF_VERIFY;
          break;
        default:
          NOTREACHED();
          return false;
      }
      const CK_FLAGS operation_flags_mask = CKF_ENCRYPT | CKF_DECRYPT |
                                            CKF_SIGN | CKF_VERIFY | CKF_WRAP |
                                            CKF_UNWRAP;
      const PK11AttrFlags attribute_flags = 0;  // Default all PK11_ATTR_ flags.

      // Note: NSS does not generate an sec_public_key if the call below fails,
      // so there is no danger of a leaked sec_public_key.
      SECKEYPublicKey* sec_public_key;
      crypto::ScopedSECKEYPrivateKey scoped_sec_private_key(
          PK11_GenerateKeyPairWithOpFlags(slot.get(),
                                          CKM_RSA_PKCS_KEY_PAIR_GEN,
                                          &rsa_gen_params,
                                          &sec_public_key,
                                          attribute_flags,
                                          operation_flags,
                                          operation_flags_mask,
                                          NULL));
      if (!private_key) {
        return false;
      }

      // One extractable input parameter is provided, and the Web Crypto API
      // spec at this time says it applies to both members of the key pair.
      // This is probably not correct: it makes more operational sense to have
      // extractable apply only to the private key and make the public key
      // always extractable. For now implement what the spec says and track the
      // spec bug here: https://www.w3.org/Bugs/Public/show_bug.cgi?id=23695
      *public_key = WebKit::WebCryptoKey::create(
          new PublicKeyHandle(crypto::ScopedSECKEYPublicKey(sec_public_key)),
          WebKit::WebCryptoKeyTypePublic,
          extractable,   // probably should be 'true' always
          algorithm,
          usage_mask);
      *private_key = WebKit::WebCryptoKey::create(
          new PrivateKeyHandle(scoped_sec_private_key.Pass()),
          WebKit::WebCryptoKeyTypePrivate,
          extractable,
          algorithm,
          usage_mask);

      return true;
    }
    default:
      return false;
  }
}

bool WebCryptoImpl::ImportKeyInternal(
    WebKit::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const WebKit::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    WebKit::WebCryptoKeyUsageMask usage_mask,
    WebKit::WebCryptoKey* key) {
  // TODO(eroman): Currently expects algorithm to always be specified, as it is
  //               required for raw format.
  if (algorithm_or_null.isNull())
    return false;
  const WebKit::WebCryptoAlgorithm& algorithm = algorithm_or_null;

  WebKit::WebCryptoKeyType type;
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac:
    case WebKit::WebCryptoAlgorithmIdAesCbc:
      type = WebKit::WebCryptoKeyTypeSecret;
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
    case WebKit::WebCryptoAlgorithmIdAesCbc: {
      mechanism = CKM_AES_CBC;
      flags |= CKF_ENCRYPT | CKF_DECRYPT;
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
    return false;
  }

  *key = WebKit::WebCryptoKey::create(new SymKeyHandle(pk11_sym_key.Pass()),
                                      type, extractable, algorithm, usage_mask);
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

bool WebCryptoImpl::VerifySignatureInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    bool* signature_match) {
  switch (algorithm.id()) {
    case WebKit::WebCryptoAlgorithmIdHmac: {
      WebKit::WebArrayBuffer result;
      if (!SignInternal(algorithm, key, data, data_size, &result)) {
        return false;
      }

      // Handling of truncated signatures is underspecified in the WebCrypto
      // spec, so here we fail verification if a truncated signature is being
      // verified.
      // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=23097
      *signature_match =
          result.byteLength() == signature_size &&
          crypto::SecureMemEqual(result.data(), signature, signature_size);

      break;
    }
    default:
      return false;
  }

  return true;
}

}  // namespace content
