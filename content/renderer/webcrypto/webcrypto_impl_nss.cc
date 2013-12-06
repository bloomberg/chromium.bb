// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <cryptohi.h>
#include <pk11pub.h>
#include <sechash.h>

#include <vector>

#include "base/logging.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace {

class SymKeyHandle : public blink::WebCryptoKeyHandle {
 public:
  explicit SymKeyHandle(crypto::ScopedPK11SymKey key) : key_(key.Pass()) {}

  PK11SymKey* key() { return key_.get(); }

 private:
  crypto::ScopedPK11SymKey key_;

  DISALLOW_COPY_AND_ASSIGN(SymKeyHandle);
};

class PublicKeyHandle : public blink::WebCryptoKeyHandle {
 public:
  explicit PublicKeyHandle(crypto::ScopedSECKEYPublicKey key)
      : key_(key.Pass()) {}

  SECKEYPublicKey* key() { return key_.get(); }

 private:
  crypto::ScopedSECKEYPublicKey key_;

  DISALLOW_COPY_AND_ASSIGN(PublicKeyHandle);
};

class PrivateKeyHandle : public blink::WebCryptoKeyHandle {
 public:
  explicit PrivateKeyHandle(crypto::ScopedSECKEYPrivateKey key)
      : key_(key.Pass()) {}

  SECKEYPrivateKey* key() { return key_.get(); }

 private:
  crypto::ScopedSECKEYPrivateKey key_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKeyHandle);
};

HASH_HashType WebCryptoAlgorithmToNSSHashType(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return HASH_AlgSHA1;
    case blink::WebCryptoAlgorithmIdSha224:
      return HASH_AlgSHA224;
    case blink::WebCryptoAlgorithmIdSha256:
      return HASH_AlgSHA256;
    case blink::WebCryptoAlgorithmIdSha384:
      return HASH_AlgSHA384;
    case blink::WebCryptoAlgorithmIdSha512:
      return HASH_AlgSHA512;
    default:
      // Not a digest algorithm.
      return HASH_AlgNULL;
  }
}

CK_MECHANISM_TYPE WebCryptoAlgorithmToHMACMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return CKM_SHA_1_HMAC;
    case blink::WebCryptoAlgorithmIdSha256:
      return CKM_SHA256_HMAC;
    default:
      // Not a supported algorithm.
      return CKM_INVALID_MECHANISM;
  }
}

bool AesCbcEncryptDecrypt(
    CK_ATTRIBUTE_TYPE operation,
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdAesCbc, algorithm.id());
  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  DCHECK(operation == CKA_ENCRYPT || operation == CKA_DECRYPT);

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  const blink::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
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

  *buffer = blink::WebArrayBuffer::create(output_max_len, 1);

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

  webcrypto::ShrinkBuffer(buffer, final_output_chunk_len + output_len);
  return true;
}

CK_MECHANISM_TYPE HmacAlgorithmToGenMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  DCHECK_EQ(algorithm.id(), blink::WebCryptoAlgorithmIdHmac);
  const blink::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
  DCHECK(params);
  switch (params->hash().id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return CKM_SHA_1_HMAC;
    case blink::WebCryptoAlgorithmIdSha256:
      return CKM_SHA256_HMAC;
    default:
      return CKM_INVALID_MECHANISM;
  }
}

CK_MECHANISM_TYPE WebCryptoAlgorithmToGenMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return CKM_AES_KEY_GEN;
    case blink::WebCryptoAlgorithmIdHmac:
      return HmacAlgorithmToGenMechanism(algorithm);
    default:
      return CKM_INVALID_MECHANISM;
  }
}

unsigned int WebCryptoHmacAlgorithmToBlockSize(
    const blink::WebCryptoAlgorithm& algorithm) {
  DCHECK_EQ(algorithm.id(), blink::WebCryptoAlgorithmIdHmac);
  const blink::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
  DCHECK(params);
  switch (params->hash().id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return 512;
    case blink::WebCryptoAlgorithmIdSha256:
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

bool IsAlgorithmRsa(const blink::WebCryptoAlgorithm& algorithm) {
  return algorithm.id() == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
         algorithm.id() == blink::WebCryptoAlgorithmIdRsaOaep ||
         algorithm.id() == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5;
}

bool ImportKeyInternalRaw(
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  DCHECK(!algorithm.isNull());

  blink::WebCryptoKeyType type;
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
    case blink::WebCryptoAlgorithmIdAesCbc:
      type = blink::WebCryptoKeyTypeSecret;
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

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacParams* params = algorithm.hmacParams();
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
    case blink::WebCryptoAlgorithmIdAesCbc: {
      mechanism = CKM_AES_CBC;
      flags |= CKF_ENCRYPT | CKF_DECRYPT;
      break;
    }
    default:
      return false;
  }

  DCHECK_NE(CKM_INVALID_MECHANISM, mechanism);
  DCHECK_NE(0ul, flags);

  SECItem key_item = {
      siBuffer,
      const_cast<unsigned char*>(key_data),
      key_data_size
  };

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

  *key = blink::WebCryptoKey::create(new SymKeyHandle(pk11_sym_key.Pass()),
                                      type, extractable, algorithm, usage_mask);
  return true;
}

bool ExportKeyInternalRaw(
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {

  DCHECK(key.handle());
  DCHECK(buffer);

  if (key.type() != blink::WebCryptoKeyTypeSecret || !key.extractable())
    return false;

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  if (PK11_ExtractKeyValue(sym_key->key()) != SECSuccess)
    return false;

  const SECItem* key_data = PK11_GetKeyData(sym_key->key());
  if (!key_data)
    return false;

  *buffer = webcrypto::CreateArrayBuffer(key_data->data, key_data->len);

  return true;
}

typedef scoped_ptr<CERTSubjectPublicKeyInfo,
                   crypto::NSSDestroyer<CERTSubjectPublicKeyInfo,
                                        SECKEY_DestroySubjectPublicKeyInfo> >
    ScopedCERTSubjectPublicKeyInfo;

// Validates an NSS KeyType against a WebCrypto algorithm. Some NSS KeyTypes
// contain enough information to fabricate a Web Crypto algorithm, which is
// returned if the input algorithm isNull(). This function indicates failure by
// returning a Null algorithm.
blink::WebCryptoAlgorithm ResolveNssKeyTypeWithInputAlgorithm(
    KeyType key_type,
    const blink::WebCryptoAlgorithm& algorithm_or_null) {
  switch (key_type) {
    case rsaKey:
      // NSS's rsaKey KeyType maps to keys with SEC_OID_PKCS1_RSA_ENCRYPTION and
      // according to RFCs 4055/5756 this can be used for both encryption and
      // signatures. However, this is not specific enough to build a compatible
      // Web Crypto algorithm, since in Web Crypto, RSA encryption and signature
      // algorithms are distinct. So if the input algorithm isNull() here, we
      // have to fail.
      if (!algorithm_or_null.isNull() && IsAlgorithmRsa(algorithm_or_null))
        return algorithm_or_null;
      break;
    case dsaKey:
    case ecKey:
    case rsaPssKey:
    case rsaOaepKey:
      // TODO(padolph): Handle other key types.
      break;
    default:
      break;
  }
  return blink::WebCryptoAlgorithm::createNull();
}

bool ImportKeyInternalSpki(
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  DCHECK(key);

  if (!key_data_size)
    return false;
  DCHECK(key_data);

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 Subject
  // Public Key Info. Decode this to a CERTSubjectPublicKeyInfo.
  SECItem spki_item = {siBuffer, const_cast<uint8*>(key_data), key_data_size};
  const ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  if (!spki)
    return false;

  crypto::ScopedSECKEYPublicKey sec_public_key(
      SECKEY_ExtractPublicKey(spki.get()));
  if (!sec_public_key)
    return false;

  const KeyType sec_key_type = SECKEY_GetPublicKeyType(sec_public_key.get());
  blink::WebCryptoAlgorithm algorithm =
      ResolveNssKeyTypeWithInputAlgorithm(sec_key_type, algorithm_or_null);
  if (algorithm.isNull())
    return false;

  *key = blink::WebCryptoKey::create(
      new PublicKeyHandle(sec_public_key.Pass()),
      blink::WebCryptoKeyTypePublic,
      extractable,
      algorithm,
      usage_mask);

  return true;
}

bool ExportKeyInternalSpki(
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {

  DCHECK(key.handle());
  DCHECK(buffer);

  if (key.type() != blink::WebCryptoKeyTypePublic || !key.extractable())
    return false;

  PublicKeyHandle* const pub_key =
      reinterpret_cast<PublicKeyHandle*>(key.handle());

  const crypto::ScopedSECItem spki_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(pub_key->key()));
  if (!spki_der)
    return false;

  DCHECK(spki_der->data);
  DCHECK(spki_der->len);

  *buffer = webcrypto::CreateArrayBuffer(spki_der->data, spki_der->len);

  return true;
}

bool ImportKeyInternalPkcs8(
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  DCHECK(key);

  if (!key_data_size)
    return false;
  DCHECK(key_data);

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 PKCS#8
  // private key info object.
  SECItem pki_der = {siBuffer, const_cast<uint8*>(key_data), key_data_size};

  SECKEYPrivateKey* seckey_private_key = NULL;
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(
          PK11_GetInternalSlot(),
          &pki_der,
          NULL,  // nickname
          NULL,  // publicValue
          false,  // isPerm
          false,  // isPrivate
          KU_ALL,  // usage
          &seckey_private_key,
          NULL) != SECSuccess) {
    return false;
  }
  DCHECK(seckey_private_key);
  crypto::ScopedSECKEYPrivateKey private_key(seckey_private_key);

  const KeyType sec_key_type = SECKEY_GetPrivateKeyType(private_key.get());
  blink::WebCryptoAlgorithm algorithm =
      ResolveNssKeyTypeWithInputAlgorithm(sec_key_type, algorithm_or_null);
  if (algorithm.isNull())
    return false;

  *key = blink::WebCryptoKey::create(
      new PrivateKeyHandle(private_key.Pass()),
      blink::WebCryptoKeyTypePrivate,
      extractable,
      algorithm,
      usage_mask);

  return true;
}

}  // namespace

void WebCryptoImpl::Init() {
  crypto::EnsureNSSInit();
}

bool WebCryptoImpl::EncryptInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebArrayBuffer* buffer) {

  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK(key.handle());
  DCHECK(buffer);

  if (algorithm.id() == blink::WebCryptoAlgorithmIdAesCbc) {
    return AesCbcEncryptDecrypt(
        CKA_ENCRYPT, algorithm, key, data, data_size, buffer);
  } else if (algorithm.id() == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5) {

    // RSAES encryption does not support empty input
    if (!data_size)
      return false;
    DCHECK(data);

    if (key.type() != blink::WebCryptoKeyTypePublic)
      return false;

    PublicKeyHandle* const public_key =
        reinterpret_cast<PublicKeyHandle*>(key.handle());

    const unsigned encrypted_length_bytes =
        SECKEY_PublicKeyStrength(public_key->key());

    // RSAES can operate on messages up to a length of k - 11, where k is the
    // octet length of the RSA modulus.
    if (encrypted_length_bytes < 11 || encrypted_length_bytes - 11 < data_size)
      return false;

    *buffer = blink::WebArrayBuffer::create(encrypted_length_bytes, 1);
    unsigned char* const buffer_data =
        reinterpret_cast<unsigned char*>(buffer->data());

    if (PK11_PubEncryptPKCS1(public_key->key(),
                             buffer_data,
                             const_cast<unsigned char*>(data),
                             data_size,
                             NULL) != SECSuccess) {
      return false;
    }
    return true;
  }

  return false;
}

bool WebCryptoImpl::DecryptInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebArrayBuffer* buffer) {

  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK(key.handle());
  DCHECK(buffer);

  if (algorithm.id() == blink::WebCryptoAlgorithmIdAesCbc) {
    return AesCbcEncryptDecrypt(
        CKA_DECRYPT, algorithm, key, data, data_size, buffer);
  } else if (algorithm.id() == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5) {

    // RSAES decryption does not support empty input
    if (!data_size)
      return false;
    DCHECK(data);

    if (key.type() != blink::WebCryptoKeyTypePrivate)
      return false;

    PrivateKeyHandle* const private_key =
        reinterpret_cast<PrivateKeyHandle*>(key.handle());

    const int modulus_length_bytes =
        PK11_GetPrivateModulusLen(private_key->key());
    if (modulus_length_bytes <= 0)
      return false;
    const unsigned max_output_length_bytes = modulus_length_bytes;

    *buffer = blink::WebArrayBuffer::create(max_output_length_bytes, 1);
    unsigned char* const buffer_data =
        reinterpret_cast<unsigned char*>(buffer->data());

    unsigned output_length_bytes = 0;
    if (PK11_PrivDecryptPKCS1(private_key->key(),
                              buffer_data,
                              &output_length_bytes,
                              max_output_length_bytes,
                              const_cast<unsigned char*>(data),
                              data_size) != SECSuccess) {
      return false;
    }
    DCHECK_LE(output_length_bytes, max_output_length_bytes);
    webcrypto::ShrinkBuffer(buffer, output_length_bytes);
    return true;
  }

  return false;
}

bool WebCryptoImpl::DigestInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    blink::WebArrayBuffer* buffer) {
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

  *buffer = blink::WebArrayBuffer::create(hash_result_length, 1);

  unsigned char* digest = reinterpret_cast<unsigned char*>(buffer->data());

  unsigned result_length = 0;
  HASH_End(context, digest, &result_length, hash_result_length);

  HASH_Destroy(context);

  return result_length == hash_result_length;
}

bool WebCryptoImpl::GenerateKeyInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  CK_MECHANISM_TYPE mech = WebCryptoAlgorithmToGenMechanism(algorithm);
  unsigned int keylen_bytes = 0;
  blink::WebCryptoKeyType key_type = blink::WebCryptoKeyTypeSecret;

  if (mech == CKM_INVALID_MECHANISM) {
    return false;
  }

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc: {
      const blink::WebCryptoAesKeyGenParams* params =
          algorithm.aesKeyGenParams();
      DCHECK(params);
      keylen_bytes = params->length() / 8;
      if (params->length() % 8)
        return false;
      key_type = blink::WebCryptoKeyTypeSecret;
      break;
    }
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
      DCHECK(params);
      if (!params->getLength(keylen_bytes)) {
        keylen_bytes = WebCryptoHmacAlgorithmToBlockSize(algorithm) / 8;
      }

      key_type = blink::WebCryptoKeyTypeSecret;
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

  *key = blink::WebCryptoKey::create(
      new SymKeyHandle(pk11_key.Pass()),
      key_type, extractable, algorithm, usage_mask);
  return true;
}

bool WebCryptoImpl::GenerateKeyPairInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* public_key,
    blink::WebCryptoKey* private_key) {

  // TODO(padolph): Handle other asymmetric algorithm key generation.
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
    case blink::WebCryptoAlgorithmIdRsaOaep:
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5: {
      const blink::WebCryptoRsaKeyGenParams* const params =
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
        case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
        case blink::WebCryptoAlgorithmIdRsaOaep:
          operation_flags = CKF_ENCRYPT | CKF_DECRYPT | CKF_WRAP | CKF_UNWRAP;
          break;
        case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
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

      *public_key = blink::WebCryptoKey::create(
          new PublicKeyHandle(crypto::ScopedSECKEYPublicKey(sec_public_key)),
          blink::WebCryptoKeyTypePublic,
          true,
          algorithm,
          usage_mask);
      *private_key = blink::WebCryptoKey::create(
          new PrivateKeyHandle(scoped_sec_private_key.Pass()),
          blink::WebCryptoKeyTypePrivate,
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
    blink::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      // A 'raw'-formatted key import requires an input algorithm.
      if (algorithm_or_null.isNull())
        return false;
      return ImportKeyInternalRaw(key_data,
                                  key_data_size,
                                  algorithm_or_null,
                                  extractable,
                                  usage_mask,
                                  key);
    case blink::WebCryptoKeyFormatSpki:
      return ImportKeyInternalSpki(key_data,
                                   key_data_size,
                                   algorithm_or_null,
                                   extractable,
                                   usage_mask,
                                   key);
    case blink::WebCryptoKeyFormatPkcs8:
      return ImportKeyInternalPkcs8(key_data,
                                    key_data_size,
                                    algorithm_or_null,
                                    extractable,
                                    usage_mask,
                                    key);
    default:
      // NOTE: blink::WebCryptoKeyFormatJwk is handled one level above.
      return false;
  }
}

bool WebCryptoImpl::ExportKeyInternal(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {
  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      return ExportKeyInternalRaw(key, buffer);
    case blink::WebCryptoKeyFormatSpki:
      return ExportKeyInternalSpki(key, buffer);
    case blink::WebCryptoKeyFormatPkcs8:
      // TODO(padolph): Implement pkcs8 export
      return false;
    default:
      return false;
  }
}

bool WebCryptoImpl::SignInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebArrayBuffer* buffer) {
  blink::WebArrayBuffer result;

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacParams* params = algorithm.hmacParams();
      if (!params) {
        return false;
      }

      SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

      DCHECK_EQ(PK11_GetMechanism(sym_key->key()),
                WebCryptoAlgorithmToHMACMechanism(params->hash()));
      DCHECK_NE(0, key.usages() & blink::WebCryptoKeyUsageSign);

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

      result = blink::WebArrayBuffer::create(signature_item.len, 1);
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
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    bool* signature_match) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac: {
      blink::WebArrayBuffer result;
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

bool WebCryptoImpl::ImportRsaPublicKeyInternal(
    const unsigned char* modulus_data,
    unsigned modulus_size,
    const unsigned char* exponent_data,
    unsigned exponent_size,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  if (!modulus_size || !exponent_size)
    return false;
  DCHECK(modulus_data);
  DCHECK(exponent_data);

  // NSS does not provide a way to create an RSA public key directly from the
  // modulus and exponent values, but it can import an DER-encoded ASN.1 blob
  // with these values and create the public key from that. The code below
  // follows the recommendation described in
  // https://developer.mozilla.org/en-US/docs/NSS/NSS_Tech_Notes/nss_tech_note7

  // Pack the input values into a struct compatible with NSS ASN.1 encoding, and
  // set up an ASN.1 encoder template for it.
  struct RsaPublicKeyData {
    SECItem modulus;
    SECItem exponent;
  };
  const RsaPublicKeyData pubkey_in = {
      {siUnsignedInteger, const_cast<unsigned char*>(modulus_data),
       modulus_size},
      {siUnsignedInteger, const_cast<unsigned char*>(exponent_data),
       exponent_size}};
  const SEC_ASN1Template rsa_public_key_template[] = {
      {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(RsaPublicKeyData)},
      {SEC_ASN1_INTEGER, offsetof(RsaPublicKeyData, modulus), },
      {SEC_ASN1_INTEGER, offsetof(RsaPublicKeyData, exponent), },
      {0, }};

  // DER-encode the public key.
  crypto::ScopedSECItem pubkey_der(SEC_ASN1EncodeItem(
      NULL, NULL, &pubkey_in, rsa_public_key_template));
  if (!pubkey_der)
    return false;

  // Import the DER-encoded public key to create an RSA SECKEYPublicKey.
  crypto::ScopedSECKEYPublicKey pubkey(
      SECKEY_ImportDERPublicKey(pubkey_der.get(), CKK_RSA));
  if (!pubkey)
    return false;

  *key = blink::WebCryptoKey::create(new PublicKeyHandle(pubkey.Pass()),
                                     blink::WebCryptoKeyTypePublic,
                                     extractable,
                                     algorithm,
                                     usage_mask);
  return true;
}

}  // namespace content
