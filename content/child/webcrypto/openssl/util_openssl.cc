// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/util_openssl.h"

#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>

#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/generate_key_result.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"

namespace content {

namespace webcrypto {

namespace {

// Exports an EVP_PKEY public key to the SPKI format.
Status ExportPKeySpki(EVP_PKEY* key, std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  crypto::ScopedBIO bio(BIO_new(BIO_s_mem()));

  // TODO(eroman): Use the OID specified by webcrypto spec.
  //               http://crbug.com/373545
  if (!i2d_PUBKEY_bio(bio.get(), key))
    return Status::ErrorUnexpected();

  char* data = NULL;
  long len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0)
    return Status::ErrorUnexpected();

  buffer->assign(data, data + len);
  return Status::Success();
}

// Exports an EVP_PKEY private key to the PKCS8 format.
Status ExportPKeyPkcs8(EVP_PKEY* key, std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  crypto::ScopedBIO bio(BIO_new(BIO_s_mem()));

  // TODO(eroman): Use the OID specified by webcrypto spec.
  //               http://crbug.com/373545
  if (!i2d_PKCS8PrivateKeyInfo_bio(bio.get(), key))
    return Status::ErrorUnexpected();

  char* data = NULL;
  long len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0)
    return Status::ErrorUnexpected();

  buffer->assign(data, data + len);
  return Status::Success();
}

}  // namespace

void PlatformInit() {
  crypto::EnsureOpenSSLInit();
}

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id) {
  switch (id) {
    case blink::WebCryptoAlgorithmIdSha1:
      return EVP_sha1();
    case blink::WebCryptoAlgorithmIdSha256:
      return EVP_sha256();
    case blink::WebCryptoAlgorithmIdSha384:
      return EVP_sha384();
    case blink::WebCryptoAlgorithmIdSha512:
      return EVP_sha512();
    default:
      return NULL;
  }
}

Status AeadEncryptDecrypt(EncryptOrDecrypt mode,
                          const std::vector<uint8_t>& raw_key,
                          const CryptoData& data,
                          unsigned int tag_length_bytes,
                          const CryptoData& iv,
                          const CryptoData& additional_data,
                          const EVP_AEAD* aead_alg,
                          std::vector<uint8_t>* buffer) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  EVP_AEAD_CTX ctx;

  if (!aead_alg)
    return Status::ErrorUnexpected();

  if (!EVP_AEAD_CTX_init(&ctx, aead_alg, vector_as_array(&raw_key),
                         raw_key.size(), tag_length_bytes, NULL)) {
    return Status::OperationError();
  }

  crypto::ScopedOpenSSL<EVP_AEAD_CTX, EVP_AEAD_CTX_cleanup> ctx_cleanup(&ctx);

  size_t len;
  int ok;

  if (mode == DECRYPT) {
    if (data.byte_length() < tag_length_bytes)
      return Status::ErrorDataTooSmall();

    buffer->resize(data.byte_length() - tag_length_bytes);

    ok = EVP_AEAD_CTX_open(&ctx, vector_as_array(buffer), &len, buffer->size(),
                           iv.bytes(), iv.byte_length(), data.bytes(),
                           data.byte_length(), additional_data.bytes(),
                           additional_data.byte_length());
  } else {
    // No need to check for unsigned integer overflow here (seal fails if
    // the output buffer is too small).
    buffer->resize(data.byte_length() + EVP_AEAD_max_overhead(aead_alg));

    ok = EVP_AEAD_CTX_seal(&ctx, vector_as_array(buffer), &len, buffer->size(),
                           iv.bytes(), iv.byte_length(), data.bytes(),
                           data.byte_length(), additional_data.bytes(),
                           additional_data.byte_length());
  }

  if (!ok)
    return Status::OperationError();
  buffer->resize(len);
  return Status::Success();
}

Status GenerateWebCryptoSecretKey(const blink::WebCryptoKeyAlgorithm& algorithm,
                                  bool extractable,
                                  blink::WebCryptoKeyUsageMask usages,
                                  unsigned int keylen_bits,
                                  GenerateKeyResult* result) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  unsigned int keylen_bytes = NumBitsToBytes(keylen_bits);
  std::vector<unsigned char> random_bytes(keylen_bytes, 0);

  if (keylen_bytes > 0) {
    if (!(RAND_bytes(&random_bytes[0], keylen_bytes)))
      return Status::OperationError();
    TruncateToBitLength(keylen_bits, &random_bytes);
  }

  result->AssignSecretKey(blink::WebCryptoKey::create(
      new SymKeyOpenSsl(CryptoData(random_bytes)),
      blink::WebCryptoKeyTypeSecret, extractable, algorithm, usages));

  return Status::Success();
}

Status CreateWebCryptoSecretKey(const CryptoData& key_data,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key) {
  *key = blink::WebCryptoKey::create(new SymKeyOpenSsl(key_data),
                                     blink::WebCryptoKeyTypeSecret, extractable,
                                     algorithm, usages);
  return Status::Success();
}

Status CreateWebCryptoPublicKey(crypto::ScopedEVP_PKEY public_key,
                                const blink::WebCryptoKeyAlgorithm& algorithm,
                                bool extractable,
                                blink::WebCryptoKeyUsageMask usages,
                                blink::WebCryptoKey* key) {
  // Serialize the key at creation time so that if structured cloning is
  // requested it can be done synchronously from the Blink thread.
  std::vector<uint8_t> spki_data;
  Status status = ExportPKeySpki(public_key.get(), &spki_data);
  if (status.IsError())
    return status;

  *key = blink::WebCryptoKey::create(
      new AsymKeyOpenSsl(public_key.Pass(), CryptoData(spki_data)),
      blink::WebCryptoKeyTypePublic, extractable, algorithm, usages);
  return Status::Success();
}

Status CreateWebCryptoPrivateKey(crypto::ScopedEVP_PKEY private_key,
                                 const blink::WebCryptoKeyAlgorithm& algorithm,
                                 bool extractable,
                                 blink::WebCryptoKeyUsageMask usages,
                                 blink::WebCryptoKey* key) {
  // Serialize the key at creation time so that if structured cloning is
  // requested it can be done synchronously from the Blink thread.
  std::vector<uint8_t> pkcs8_data;
  Status status = ExportPKeyPkcs8(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  *key = blink::WebCryptoKey::create(
      new AsymKeyOpenSsl(private_key.Pass(), CryptoData(pkcs8_data)),
      blink::WebCryptoKeyTypePrivate, extractable, algorithm, usages);
  return Status::Success();
}

Status ImportUnverifiedPkeyFromSpki(const CryptoData& key_data,
                                    int expected_pkey_id,
                                    crypto::ScopedEVP_PKEY* pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const uint8_t* ptr = key_data.bytes();
  pkey->reset(d2i_PUBKEY(nullptr, &ptr, key_data.byte_length()));
  if (!pkey->get() || ptr != key_data.bytes() + key_data.byte_length())
    return Status::DataError();

  if (EVP_PKEY_id(pkey->get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  return Status::Success();
}

Status ImportUnverifiedPkeyFromPkcs8(const CryptoData& key_data,
                                     int expected_pkey_id,
                                     crypto::ScopedEVP_PKEY* pkey) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const uint8_t* ptr = key_data.bytes();
  crypto::ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free> p8inf(
      d2i_PKCS8_PRIV_KEY_INFO(nullptr, &ptr, key_data.byte_length()));
  if (!p8inf.get() || ptr != key_data.bytes() + key_data.byte_length())
    return Status::DataError();

  pkey->reset(EVP_PKCS82PKEY(p8inf.get()));
  if (!pkey->get())
    return Status::DataError();

  if (EVP_PKEY_id(pkey->get()) != expected_pkey_id)
    return Status::DataError();  // Data did not define expected key type.

  return Status::Success();
}

BIGNUM* CreateBIGNUM(const std::string& n) {
  return BN_bin2bn(reinterpret_cast<const uint8_t*>(n.data()), n.size(), NULL);
}

std::vector<uint8_t> BIGNUMToVector(const BIGNUM* n) {
  std::vector<uint8_t> v(BN_num_bytes(n));
  BN_bn2bin(n, vector_as_array(&v));
  return v;
}

}  // namespace webcrypto

}  // namespace content
