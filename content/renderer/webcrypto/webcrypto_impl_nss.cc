// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <cryptohi.h>
#include <pk11pub.h>
#include <sechash.h>

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

#if defined(USE_NSS)
#include <dlfcn.h>
#endif

// At the time of this writing:
//   * Windows and Mac builds ship with their own copy of NSS (3.15+)
//   * Linux builds use the system's libnss, which is 3.14 on Debian (but 3.15+
//     on other distros).
//
// Since NSS provides AES-GCM support starting in version 3.15, it may be
// unavailable for Linux Chrome users.
//
//  * !defined(CKM_AES_GCM)
//
//      This means that at build time, the NSS header pkcs11t.h is older than
//      3.15. However at runtime support may be present.
//
//  * !defined(USE_NSS)
//
//      This means that Chrome is being built with an embedded copy of NSS,
//      which can be assumed to be >= 3.15. On the other hand if USE_NSS is
//      defined, it also implies running on Linux.
//
// TODO(eroman): Simplify this once 3.15+ is required by Linux builds.
#if !defined(CKM_AES_GCM)
#define CKM_AES_GCM 0x00001087

struct CK_GCM_PARAMS {
  CK_BYTE_PTR pIv;
  CK_ULONG ulIvLen;
  CK_BYTE_PTR pAAD;
  CK_ULONG ulAADLen;
  CK_ULONG ulTagBits;
};
#endif  // !defined(CKM_AES_GCM)

// Signature for PK11_Encrypt and PK11_Decrypt.
typedef SECStatus
(*PK11_EncryptDecryptFunction)(
    PK11SymKey*, CK_MECHANISM_TYPE, SECItem*,
    unsigned char*, unsigned int*, unsigned int,
    const unsigned char*, unsigned int);

// Singleton to abstract away dynamically loading libnss3.so
class AesGcmSupport {
 public:
  bool IsSupported() const {
    return pk11_encrypt_func_ && pk11_decrypt_func_;
  }

  // Returns NULL if unsupported.
  PK11_EncryptDecryptFunction pk11_encrypt_func() const {
    return pk11_encrypt_func_;
  }

  // Returns NULL if unsupported.
  PK11_EncryptDecryptFunction pk11_decrypt_func() const {
    return pk11_decrypt_func_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<AesGcmSupport>;

  AesGcmSupport() {
#if !defined(USE_NSS)
    // Using a bundled version of NSS that is guaranteed to have this symbol.
    pk11_encrypt_func_ = PK11_Encrypt;
    pk11_decrypt_func_ = PK11_Decrypt;
#else
    // Using system NSS libraries and PCKS #11 modules, which may not have the
    // necessary function (PK11_Encrypt) or mechanism support (CKM_AES_GCM).

    // If PK11_Encrypt() was successfully resolved, then NSS will support
    // AES-GCM directly. This was introduced in NSS 3.15.
    pk11_encrypt_func_ =
        reinterpret_cast<PK11_EncryptDecryptFunction>(
            dlsym(RTLD_DEFAULT, "PK11_Encrypt"));
    pk11_decrypt_func_ =
        reinterpret_cast<PK11_EncryptDecryptFunction>(
            dlsym(RTLD_DEFAULT, "PK11_Decrypt"));
#endif
  }

  PK11_EncryptDecryptFunction pk11_encrypt_func_;
  PK11_EncryptDecryptFunction pk11_decrypt_func_;
};

base::LazyInstance<AesGcmSupport>::Leaky g_aes_gcm_support =
    LAZY_INSTANCE_INITIALIZER;

namespace content {

using webcrypto::Status;

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

CK_MECHANISM_TYPE WebCryptoHashToHMACMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      return CKM_SHA_1_HMAC;
    case blink::WebCryptoAlgorithmIdSha224:
      return CKM_SHA224_HMAC;
    case blink::WebCryptoAlgorithmIdSha256:
      return CKM_SHA256_HMAC;
    case blink::WebCryptoAlgorithmIdSha384:
      return CKM_SHA384_HMAC;
    case blink::WebCryptoAlgorithmIdSha512:
      return CKM_SHA512_HMAC;
    default:
      // Not a supported algorithm.
      return CKM_INVALID_MECHANISM;
  }
}

Status AesCbcEncryptDecrypt(
    CK_ATTRIBUTE_TYPE operation,
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdAesCbc, algorithm.id());
  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  DCHECK(operation == CKA_ENCRYPT || operation == CKA_DECRYPT);

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  const blink::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
  if (params->iv().size() != AES_BLOCK_SIZE)
    return Status::ErrorIncorrectSizeAesCbcIv();

  SECItem iv_item;
  iv_item.type = siBuffer;
  iv_item.data = const_cast<unsigned char*>(params->iv().data());
  iv_item.len = params->iv().size();

  crypto::ScopedSECItem param(PK11_ParamFromIV(CKM_AES_CBC_PAD, &iv_item));
  if (!param)
    return Status::Error();

  crypto::ScopedPK11Context context(PK11_CreateContextBySymKey(
      CKM_AES_CBC_PAD, operation, sym_key->key(), param.get()));

  if (!context.get())
    return Status::Error();

  // Oddly PK11_CipherOp takes input and output lengths as "int" rather than
  // "unsigned int". Do some checks now to avoid integer overflowing.
  if (data_size >= INT_MAX - AES_BLOCK_SIZE) {
    // TODO(eroman): Handle this by chunking the input fed into NSS. Right now
    // it doesn't make much difference since the one-shot API would end up
    // blowing out the memory and crashing anyway.
    return Status::ErrorDataTooLarge();
  }

  // PK11_CipherOp does an invalid memory access when given empty decryption
  // input, or input which is not a multiple of the block size. See also
  // https://bugzilla.mozilla.com/show_bug.cgi?id=921687.
  if (operation == CKA_DECRYPT &&
      (data_size == 0 || (data_size % AES_BLOCK_SIZE != 0))) {
    return Status::Error();
  }

  // TODO(eroman): Refine the output buffer size. It can be computed exactly for
  //               encryption, and can be smaller for decryption.
  unsigned int output_max_len = data_size + AES_BLOCK_SIZE;
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
    return Status::Error();
  }

  unsigned int final_output_chunk_len;
  if (SECSuccess != PK11_DigestFinal(context.get(),
                                     buffer_data + output_len,
                                     &final_output_chunk_len,
                                     output_max_len - output_len)) {
    return Status::Error();
  }

  webcrypto::ShrinkBuffer(buffer, final_output_chunk_len + output_len);
  return Status::Success();
}

// Helper to either encrypt or decrypt for AES-GCM. The result of encryption is
// the concatenation of the ciphertext and the authentication tag. Similarly,
// this is the expectation for the input to decryption.
Status AesGcmEncryptDecrypt(
    bool encrypt,
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdAesGcm, algorithm.id());
  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK_EQ(blink::WebCryptoKeyTypeSecret, key.type());

  if (!g_aes_gcm_support.Get().IsSupported())
    return Status::ErrorUnsupported();

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  const blink::WebCryptoAesGcmParams* params = algorithm.aesGcmParams();
  if (!params)
    return Status::ErrorUnexpected();

  // TODO(eroman): The spec doesn't define the default value. Assume 128 for now
  // since that is the maximum tag length:
  // http://www.w3.org/2012/webcrypto/track/issues/46
  unsigned int tag_length_bits = 128;
  if (params->hasTagLengthBits())
    tag_length_bits = params->optionalTagLengthBits();

  if (tag_length_bits > 128 || (tag_length_bits % 8) != 0)
    return Status::ErrorInvalidAesGcmTagLength();

  unsigned int tag_length_bytes = tag_length_bits / 8;

  CK_GCM_PARAMS gcm_params = {0};
  gcm_params.pIv =
      const_cast<unsigned char*>(algorithm.aesGcmParams()->iv().data());
  gcm_params.ulIvLen = algorithm.aesGcmParams()->iv().size();

  gcm_params.pAAD =
      const_cast<unsigned char*>(params->optionalAdditionalData().data());
  gcm_params.ulAADLen = params->optionalAdditionalData().size();

  gcm_params.ulTagBits = tag_length_bits;

  SECItem param;
  param.type = siBuffer;
  param.data = reinterpret_cast<unsigned char*>(&gcm_params);
  param.len = sizeof(gcm_params);

  unsigned int buffer_size = 0;

  // Calculate the output buffer size.
  if (encrypt) {
    // TODO(eroman): This is ugly, abstract away the safe integer arithmetic.
    if (data_size > (UINT_MAX - tag_length_bytes))
      return Status::ErrorDataTooLarge();
    buffer_size = data_size + tag_length_bytes;
  } else {
    // TODO(eroman): In theory the buffer allocated for the plain text should be
    // sized as |data_size - tag_length_bytes|.
    //
    // However NSS has a bug whereby it will fail if the output buffer size is
    // not at least as large as the ciphertext:
    //
    // https://bugzilla.mozilla.org/show_bug.cgi?id=%20853674
    //
    // From the analysis of that bug it looks like it might be safe to pass a
    // correctly sized buffer but lie about its size. Since resizing the
    // WebCryptoArrayBuffer is expensive that hack may be worth looking into.
    buffer_size = data_size;
  }

  *buffer = blink::WebArrayBuffer::create(buffer_size, 1);
  unsigned char* buffer_data = reinterpret_cast<unsigned char*>(buffer->data());

  PK11_EncryptDecryptFunction func =
    encrypt ? g_aes_gcm_support.Get().pk11_encrypt_func() :
              g_aes_gcm_support.Get().pk11_decrypt_func();

  unsigned int output_len = 0;
  SECStatus result = func(sym_key->key(), CKM_AES_GCM, &param,
                          buffer_data, &output_len, buffer->byteLength(),
                          data, data_size);

  if (result != SECSuccess)
    return Status::Error();

  // Unfortunately the buffer needs to be shrunk for decryption (see the NSS bug
  // above).
  webcrypto::ShrinkBuffer(buffer, output_len);

  return Status::Success();
}

CK_MECHANISM_TYPE WebCryptoAlgorithmToGenMechanism(
    const blink::WebCryptoAlgorithm& algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesKw:
      return CKM_AES_KEY_GEN;
    case blink::WebCryptoAlgorithmIdHmac:
      return WebCryptoHashToHMACMechanism(algorithm.hmacKeyParams()->hash());
    default:
      return CKM_INVALID_MECHANISM;
  }
}

// Converts a (big-endian) WebCrypto BigInteger, with or without leading zeros,
// to unsigned long.
bool BigIntegerToLong(const uint8* data,
                      unsigned int data_size,
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

Status ImportKeyInternalRaw(
    const unsigned char* key_data,
    unsigned int key_data_size,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  DCHECK(!algorithm.isNull());

  blink::WebCryptoKeyType type;
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesKw:
    case blink::WebCryptoAlgorithmIdAesGcm:
      type = blink::WebCryptoKeyTypeSecret;
      break;
    // TODO(bryaneyler): Support more key types.
    default:
      return Status::ErrorUnsupported();
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
      if (!params)
        return Status::ErrorUnexpected();

      mechanism = WebCryptoHashToHMACMechanism(params->hash());
      if (mechanism == CKM_INVALID_MECHANISM)
        return Status::ErrorUnsupported();

      flags |= CKF_SIGN | CKF_VERIFY;

      break;
    }
    case blink::WebCryptoAlgorithmIdAesCbc: {
      mechanism = CKM_AES_CBC;
      flags |= CKF_ENCRYPT | CKF_DECRYPT;
      break;
    }
    case blink::WebCryptoAlgorithmIdAesKw: {
      mechanism = CKM_NSS_AES_KEY_WRAP;
      flags |= CKF_WRAP | CKF_WRAP;
      break;
    }
    case blink::WebCryptoAlgorithmIdAesGcm: {
      if (!g_aes_gcm_support.Get().IsSupported())
        return Status::ErrorUnsupported();
      mechanism = CKM_AES_GCM;
      flags |= CKF_ENCRYPT | CKF_DECRYPT;
      break;
    }
    default:
      return Status::ErrorUnsupported();
  }

  DCHECK_NE(CKM_INVALID_MECHANISM, mechanism);
  DCHECK_NE(0ul, flags);

  SECItem key_item = {
      siBuffer,
      const_cast<unsigned char*>(key_data),
      key_data_size
  };

  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  crypto::ScopedPK11SymKey pk11_sym_key(
      PK11_ImportSymKeyWithFlags(slot.get(),
                                 mechanism,
                                 PK11_OriginUnwrap,
                                 CKA_FLAGS_ONLY,
                                 &key_item,
                                 flags,
                                 false,
                                 NULL));
  if (!pk11_sym_key.get())
    return Status::Error();

  *key = blink::WebCryptoKey::create(new SymKeyHandle(pk11_sym_key.Pass()),
                                      type, extractable, algorithm, usage_mask);
  return Status::Success();
}

Status ExportKeyInternalRaw(
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {

  DCHECK(key.handle());
  DCHECK(buffer);

  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();
  if (key.type() != blink::WebCryptoKeyTypeSecret)
    return Status::ErrorUnexpectedKeyType();

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  if (PK11_ExtractKeyValue(sym_key->key()) != SECSuccess)
    return Status::Error();

  const SECItem* key_data = PK11_GetKeyData(sym_key->key());
  if (!key_data)
    return Status::Error();

  *buffer = webcrypto::CreateArrayBuffer(key_data->data, key_data->len);

  return Status::Success();
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

Status ImportKeyInternalSpki(
    const unsigned char* key_data,
    unsigned int key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  DCHECK(key);

  if (!key_data_size)
    return Status::ErrorImportEmptyKeyData();
  DCHECK(key_data);

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 Subject
  // Public Key Info. Decode this to a CERTSubjectPublicKeyInfo.
  SECItem spki_item = {siBuffer, const_cast<uint8*>(key_data), key_data_size};
  const ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
  if (!spki)
    return Status::Error();

  crypto::ScopedSECKEYPublicKey sec_public_key(
      SECKEY_ExtractPublicKey(spki.get()));
  if (!sec_public_key)
    return Status::Error();

  const KeyType sec_key_type = SECKEY_GetPublicKeyType(sec_public_key.get());
  blink::WebCryptoAlgorithm algorithm =
      ResolveNssKeyTypeWithInputAlgorithm(sec_key_type, algorithm_or_null);
  if (algorithm.isNull())
    return Status::Error();

  *key = blink::WebCryptoKey::create(
      new PublicKeyHandle(sec_public_key.Pass()),
      blink::WebCryptoKeyTypePublic,
      extractable,
      algorithm,
      usage_mask);

  return Status::Success();
}

Status ExportKeyInternalSpki(
    const blink::WebCryptoKey& key,
    blink::WebArrayBuffer* buffer) {

  DCHECK(key.handle());
  DCHECK(buffer);

  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  PublicKeyHandle* const pub_key =
      reinterpret_cast<PublicKeyHandle*>(key.handle());

  const crypto::ScopedSECItem spki_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(pub_key->key()));
  if (!spki_der)
    return Status::Error();

  DCHECK(spki_der->data);
  DCHECK(spki_der->len);

  *buffer = webcrypto::CreateArrayBuffer(spki_der->data, spki_der->len);

  return Status::Success();
}

Status ImportKeyInternalPkcs8(
    const unsigned char* key_data,
    unsigned int key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  DCHECK(key);

  if (!key_data_size)
    return Status::ErrorImportEmptyKeyData();
  DCHECK(key_data);

  // The binary blob 'key_data' is expected to be a DER-encoded ASN.1 PKCS#8
  // private key info object.
  SECItem pki_der = {siBuffer, const_cast<uint8*>(key_data), key_data_size};

  SECKEYPrivateKey* seckey_private_key = NULL;
  crypto::ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (PK11_ImportDERPrivateKeyInfoAndReturnKey(
          slot.get(),
          &pki_der,
          NULL,  // nickname
          NULL,  // publicValue
          false,  // isPerm
          false,  // isPrivate
          KU_ALL,  // usage
          &seckey_private_key,
          NULL) != SECSuccess) {
    return Status::Error();
  }
  DCHECK(seckey_private_key);
  crypto::ScopedSECKEYPrivateKey private_key(seckey_private_key);

  const KeyType sec_key_type = SECKEY_GetPrivateKeyType(private_key.get());
  blink::WebCryptoAlgorithm algorithm =
      ResolveNssKeyTypeWithInputAlgorithm(sec_key_type, algorithm_or_null);
  if (algorithm.isNull())
    return Status::Error();

  *key = blink::WebCryptoKey::create(
      new PrivateKeyHandle(private_key.Pass()),
      blink::WebCryptoKeyTypePrivate,
      extractable,
      algorithm,
      usage_mask);

  return Status::Success();
}

// -----------------------------------
// Hmac
// -----------------------------------

Status SignHmac(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());

  const blink::WebCryptoHmacParams* params = algorithm.hmacParams();
  if (!params)
    return Status::ErrorUnexpected();

  SymKeyHandle* sym_key = reinterpret_cast<SymKeyHandle*>(key.handle());

  DCHECK_EQ(PK11_GetMechanism(sym_key->key()),
            WebCryptoHashToHMACMechanism(params->hash()));

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
    return Status::Error();
  }

  DCHECK_NE(0u, signature_item.len);

  *buffer = blink::WebArrayBuffer::create(signature_item.len, 1);
  signature_item.data = reinterpret_cast<unsigned char*>(buffer->data());

  if (PK11_SignWithSymKey(sym_key->key(),
                          PK11_GetMechanism(sym_key->key()),
                          &param_item,
                          &signature_item,
                          &data_item) != SECSuccess) {
    return Status::Error();
  }

  DCHECK_EQ(buffer->byteLength(), signature_item.len);
  return Status::Success();
}

Status VerifyHmac(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned int signature_size,
    const unsigned char* data,
    unsigned int data_size,
    bool* signature_match) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());

  blink::WebArrayBuffer result;
  Status status = SignHmac(algorithm, key, data, data_size, &result);
  if (status.IsError())
    return status;

  // Handling of truncated signatures is underspecified in the WebCrypto
  // spec, so here we fail verification if a truncated signature is being
  // verified.
  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=23097
  *signature_match =
    result.byteLength() == signature_size &&
    crypto::SecureMemEqual(result.data(), signature, signature_size);

  return Status::Success();
}

// -----------------------------------
// RsaEsPkcs1v1_5
// -----------------------------------

Status EncryptRsaEsPkcs1v1_5(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, algorithm.id());

  // RSAES encryption does not support empty input
  if (!data_size)
    return Status::Error();
  DCHECK(data);

  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  PublicKeyHandle* const public_key =
      reinterpret_cast<PublicKeyHandle*>(key.handle());

  const unsigned int encrypted_length_bytes =
      SECKEY_PublicKeyStrength(public_key->key());

  // RSAES can operate on messages up to a length of k - 11, where k is the
  // octet length of the RSA modulus.
  if (encrypted_length_bytes < 11 || encrypted_length_bytes - 11 < data_size)
    return Status::ErrorDataTooLarge();

  *buffer = blink::WebArrayBuffer::create(encrypted_length_bytes, 1);
  unsigned char* const buffer_data =
      reinterpret_cast<unsigned char*>(buffer->data());

  if (PK11_PubEncryptPKCS1(public_key->key(),
                           buffer_data,
                           const_cast<unsigned char*>(data),
                           data_size,
                           NULL) != SECSuccess) {
    return Status::Error();
  }
  return Status::Success();
}

Status DecryptRsaEsPkcs1v1_5(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, algorithm.id());

  // RSAES decryption does not support empty input
  if (!data_size)
    return Status::Error();
  DCHECK(data);

  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  PrivateKeyHandle* const private_key =
      reinterpret_cast<PrivateKeyHandle*>(key.handle());

  const int modulus_length_bytes =
      PK11_GetPrivateModulusLen(private_key->key());
  if (modulus_length_bytes <= 0)
    return Status::ErrorUnexpected();
  const unsigned int max_output_length_bytes = modulus_length_bytes;

  *buffer = blink::WebArrayBuffer::create(max_output_length_bytes, 1);
  unsigned char* const buffer_data =
      reinterpret_cast<unsigned char*>(buffer->data());

  unsigned int output_length_bytes = 0;
  if (PK11_PrivDecryptPKCS1(private_key->key(),
                            buffer_data,
                            &output_length_bytes,
                            max_output_length_bytes,
                            const_cast<unsigned char*>(data),
                            data_size) != SECSuccess) {
    return Status::Error();
  }
  DCHECK_LE(output_length_bytes, max_output_length_bytes);
  webcrypto::ShrinkBuffer(buffer, output_length_bytes);
  return Status::Success();
}

// -----------------------------------
// RsaSsaPkcs1v1_5
// -----------------------------------

Status SignRsaSsaPkcs1v1_5(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, algorithm.id());

  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();

  if (webcrypto::GetInnerHashAlgorithm(algorithm).isNull())
    return Status::ErrorUnexpected();

  PrivateKeyHandle* const private_key =
      reinterpret_cast<PrivateKeyHandle*>(key.handle());
  DCHECK(private_key);
  DCHECK(private_key->key());

  // Pick the NSS signing algorithm by combining RSA-SSA (RSA PKCS1) and the
  // inner hash of the input Web Crypto algorithm.
  SECOidTag sign_alg_tag;
  switch (webcrypto::GetInnerHashAlgorithm(algorithm).id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      sign_alg_tag = SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
      break;
    case blink::WebCryptoAlgorithmIdSha224:
      sign_alg_tag = SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION;
      break;
    case blink::WebCryptoAlgorithmIdSha256:
      sign_alg_tag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
      break;
    case blink::WebCryptoAlgorithmIdSha384:
      sign_alg_tag = SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
      break;
    case blink::WebCryptoAlgorithmIdSha512:
      sign_alg_tag = SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
      break;
    default:
      return Status::ErrorUnsupported();
  }

  crypto::ScopedSECItem signature_item(SECITEM_AllocItem(NULL, NULL, 0));
  if (SEC_SignData(signature_item.get(),
                   data,
                   data_size,
                   private_key->key(),
                   sign_alg_tag) != SECSuccess) {
    return Status::Error();
  }

  *buffer = webcrypto::CreateArrayBuffer(signature_item->data,
                                         signature_item->len);
  return Status::Success();
}

Status VerifyRsaSsaPkcs1v1_5(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned int signature_size,
    const unsigned char* data,
    unsigned int data_size,
    bool* signature_match) {
  DCHECK_EQ(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, algorithm.id());

  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();

  PublicKeyHandle* const public_key =
      reinterpret_cast<PublicKeyHandle*>(key.handle());
  DCHECK(public_key);
  DCHECK(public_key->key());

  const SECItem signature_item = {
      siBuffer,
      const_cast<unsigned char*>(signature),
      signature_size
  };

  SECOidTag hash_alg_tag;
  switch (webcrypto::GetInnerHashAlgorithm(algorithm).id()) {
    case blink::WebCryptoAlgorithmIdSha1:
      hash_alg_tag = SEC_OID_SHA1;
      break;
    case blink::WebCryptoAlgorithmIdSha224:
      hash_alg_tag = SEC_OID_SHA224;
      break;
    case blink::WebCryptoAlgorithmIdSha256:
      hash_alg_tag = SEC_OID_SHA256;
      break;
    case blink::WebCryptoAlgorithmIdSha384:
      hash_alg_tag = SEC_OID_SHA384;
      break;
    case blink::WebCryptoAlgorithmIdSha512:
      hash_alg_tag = SEC_OID_SHA512;
      break;
    default:
      return Status::ErrorUnsupported();
  }

  *signature_match =
      SECSuccess == VFY_VerifyDataDirect(data,
                                         data_size,
                                         public_key->key(),
                                         &signature_item,
                                         SEC_OID_PKCS1_RSA_ENCRYPTION,
                                         hash_alg_tag,
                                         NULL,
                                         NULL);
  return Status::Success();
}

// -----------------------------------
// Key generation
// -----------------------------------

Status GenerateRsaKeyPair(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* public_key,
    blink::WebCryptoKey* private_key) {
  const blink::WebCryptoRsaKeyGenParams* const params =
      algorithm.rsaKeyGenParams();
  DCHECK(params);

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot)
    return Status::Error();

  unsigned long public_exponent;
  if (!params->modulusLengthBits())
    return Status::ErrorGenerateRsaZeroModulus();

  if (!BigIntegerToLong(params->publicExponent().data(),
                        params->publicExponent().size(),
                        &public_exponent) || !public_exponent) {
    return Status::ErrorGenerateKeyPublicExponent();
  }

  PK11RSAGenParams rsa_gen_params;
  rsa_gen_params.keySizeInBits = params->modulusLengthBits();
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
      return Status::ErrorUnexpected();
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
  if (!private_key)
    return Status::Error();

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

  return Status::Success();
}

// Get the secret key length in bytes from generation parameters. This resolves
// any defaults.
Status GetGenerateSecretKeyLength(const blink::WebCryptoAlgorithm& algorithm,
                                  unsigned int* keylen_bytes) {
  *keylen_bytes = 0;

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesKw: {
      const blink::WebCryptoAesKeyGenParams* params =
          algorithm.aesKeyGenParams();
      DCHECK(params);
      // Ensure the key length is a multiple of 8 bits. Let NSS verify further
      // algorithm-specific length restrictions.
      if (params->lengthBits() % 8)
        return Status::ErrorGenerateKeyLength();
      *keylen_bytes = params->lengthBits() / 8;
      break;
    }
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
      DCHECK(params);
      if (params->hasLengthBytes())
        *keylen_bytes = params->optionalLengthBytes();
      else
        *keylen_bytes = webcrypto::ShaBlockSizeBytes(params->hash().id());
      break;
    }

    default:
      return Status::ErrorUnsupported();
  }

  if (*keylen_bytes == 0)
    return Status::ErrorGenerateKeyLength();

  return Status::Success();
}

}  // namespace

void WebCryptoImpl::Init() {
  crypto::EnsureNSSInit();
}

Status WebCryptoImpl::EncryptInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {

  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK(key.handle());
  DCHECK(buffer);

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return AesCbcEncryptDecrypt(
          CKA_ENCRYPT, algorithm, key, data, data_size, buffer);
    case blink::WebCryptoAlgorithmIdAesGcm:
      return AesGcmEncryptDecrypt(
          true, algorithm, key, data, data_size, buffer);
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
      return EncryptRsaEsPkcs1v1_5(algorithm, key, data, data_size, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::DecryptInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {

  DCHECK_EQ(algorithm.id(), key.algorithm().id());
  DCHECK(key.handle());
  DCHECK(buffer);

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return AesCbcEncryptDecrypt(
          CKA_DECRYPT, algorithm, key, data, data_size, buffer);
    case blink::WebCryptoAlgorithmIdAesGcm:
      return AesGcmEncryptDecrypt(
          false, algorithm, key, data, data_size, buffer);
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
      return DecryptRsaEsPkcs1v1_5(algorithm, key, data, data_size, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::DigestInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {
  HASH_HashType hash_type = WebCryptoAlgorithmToNSSHashType(algorithm);
  if (hash_type == HASH_AlgNULL)
    return Status::ErrorUnsupported();

  HASHContext* context = HASH_Create(hash_type);
  if (!context)
    return Status::Error();

  HASH_Begin(context);

  HASH_Update(context, data, data_size);

  unsigned int hash_result_length = HASH_ResultLenContext(context);
  DCHECK_LE(hash_result_length, static_cast<size_t>(HASH_LENGTH_MAX));

  *buffer = blink::WebArrayBuffer::create(hash_result_length, 1);

  unsigned char* digest = reinterpret_cast<unsigned char*>(buffer->data());

  unsigned int result_length = 0;
  HASH_End(context, digest, &result_length, hash_result_length);

  HASH_Destroy(context);

  if (result_length != hash_result_length)
    return Status::ErrorUnexpected();
  return Status::Success();
}

Status WebCryptoImpl::GenerateSecretKeyInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  CK_MECHANISM_TYPE mech = WebCryptoAlgorithmToGenMechanism(algorithm);
  blink::WebCryptoKeyType key_type = blink::WebCryptoKeyTypeSecret;

  if (mech == CKM_INVALID_MECHANISM)
    return Status::ErrorUnsupported();

  unsigned int keylen_bytes = 0;
  Status status = GetGenerateSecretKeyLength(algorithm, &keylen_bytes);
  if (status.IsError())
    return status;

  crypto::ScopedPK11Slot slot(PK11_GetInternalKeySlot());
  if (!slot)
    return Status::Error();

  crypto::ScopedPK11SymKey pk11_key(
      PK11_KeyGen(slot.get(), mech, NULL, keylen_bytes, NULL));

  if (!pk11_key)
    return Status::Error();

  *key = blink::WebCryptoKey::create(
      new SymKeyHandle(pk11_key.Pass()),
      key_type, extractable, algorithm, usage_mask);
  return Status::Success();
}

Status WebCryptoImpl::GenerateKeyPairInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* public_key,
    blink::WebCryptoKey* private_key) {

  // TODO(padolph): Handle other asymmetric algorithm key generation.
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
    case blink::WebCryptoAlgorithmIdRsaOaep:
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return GenerateRsaKeyPair(algorithm, extractable, usage_mask,
                                public_key, private_key);
    default:
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::ImportKeyInternal(
    blink::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned int key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      // A 'raw'-formatted key import requires an input algorithm.
      if (algorithm_or_null.isNull())
        return Status::ErrorMissingAlgorithmImportRawKey();
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
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::ExportKeyInternal(
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
      return Status::ErrorUnsupported();
    default:
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::SignInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned int data_size,
    blink::WebArrayBuffer* buffer) {

  // Note: It is not an error to sign empty data.

  DCHECK(buffer);
  DCHECK_NE(0, key.usages() & blink::WebCryptoKeyUsageSign);

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      return SignHmac(algorithm, key, data, data_size, buffer);
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return SignRsaSsaPkcs1v1_5(algorithm, key, data, data_size, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::VerifySignatureInternal(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned int signature_size,
    const unsigned char* data,
    unsigned int data_size,
    bool* signature_match) {

  if (!signature_size) {
    // None of the algorithms generate valid zero-length signatures so this
    // will necessarily fail verification. Early return to protect
    // implementations from dealing with a NULL signature pointer.
    *signature_match = false;
    return Status::Success();
  }

  DCHECK(signature);

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      return VerifyHmac(algorithm, key, signature, signature_size,
                        data, data_size, signature_match);
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return VerifyRsaSsaPkcs1v1_5(algorithm, key, signature, signature_size,
                                   data, data_size, signature_match);
    default:
      return Status::ErrorUnsupported();
  }
}

Status WebCryptoImpl::ImportRsaPublicKeyInternal(
    const unsigned char* modulus_data,
    unsigned int modulus_size,
    const unsigned char* exponent_data,
    unsigned int exponent_size,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  if (!modulus_size)
    return Status::ErrorImportRsaEmptyModulus();

  if (!exponent_size)
    return Status::ErrorImportRsaEmptyExponent();

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
    return Status::Error();

  // Import the DER-encoded public key to create an RSA SECKEYPublicKey.
  crypto::ScopedSECKEYPublicKey pubkey(
      SECKEY_ImportDERPublicKey(pubkey_der.get(), CKK_RSA));
  if (!pubkey)
    return Status::Error();

  *key = blink::WebCryptoKey::create(new PublicKeyHandle(pubkey.Pass()),
                                     blink::WebCryptoKeyTypePublic,
                                     extractable,
                                     algorithm,
                                     usage_mask);
  return Status::Success();
}

}  // namespace content
