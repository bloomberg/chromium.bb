// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/rsa_key_openssl.h"

#include <openssl/evp.h>
#include <openssl/pkcs12.h>

#include "base/logging.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

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

// Creates a blink::WebCryptoAlgorithm having the modulus length and public
// exponent  of |key|.
Status CreateRsaHashedKeyAlgorithm(
    blink::WebCryptoAlgorithmId rsa_algorithm,
    blink::WebCryptoAlgorithmId hash_algorithm,
    EVP_PKEY* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  DCHECK(IsAlgorithmRsa(rsa_algorithm));
  DCHECK_EQ(EVP_PKEY_RSA, EVP_PKEY_id(key));

  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(key));
  if (!rsa.get())
    return Status::ErrorUnexpected();

  unsigned int modulus_length_bits = BN_num_bits(rsa.get()->n);

  // Convert the public exponent to big-endian representation.
  std::vector<uint8_t> e(BN_num_bytes(rsa.get()->e));
  if (e.size() == 0)
    return Status::ErrorUnexpected();
  if (e.size() != BN_bn2bin(rsa.get()->e, &e[0]))
    return Status::ErrorUnexpected();

  *key_algorithm = blink::WebCryptoKeyAlgorithm::createRsaHashed(
      rsa_algorithm, modulus_length_bits, &e[0], e.size(), hash_algorithm);

  return Status::Success();
}

// Verifies that |key| is consistent with the input algorithm id, and creates a
// blink::WebCryptoKeyAlgorithm describing the key.
// Returns Status::Success() on success and sets |*key_algorithm|.
Status ValidateKeyTypeAndCreateKeyAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm,
    EVP_PKEY* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400
  if (EVP_PKEY_id(key) != EVP_PKEY_RSA)
    return Status::DataError();  // Data did not define an RSA key.
  return CreateRsaHashedKeyAlgorithm(algorithm.id(),
                                     GetInnerHashAlgorithm(algorithm).id(),
                                     key,
                                     key_algorithm);
}

}  // namespace

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usage_mask) const {
  switch (format) {
    case blink::WebCryptoKeyFormatSpki:
      return CheckKeyCreationUsages(all_public_key_usages_, usage_mask);
    case blink::WebCryptoKeyFormatPkcs8:
      return CheckKeyCreationUsages(all_private_key_usages_, usage_mask);
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status RsaHashedAlgorithm::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedBIO bio(BIO_new_mem_buf(const_cast<uint8_t*>(key_data.bytes()),
                                        key_data.byte_length()));
  if (!bio.get())
    return Status::ErrorUnexpected();

  crypto::ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free>::Type
      p8inf(d2i_PKCS8_PRIV_KEY_INFO_bio(bio.get(), NULL));
  if (!p8inf.get())
    return Status::DataError();

  crypto::ScopedEVP_PKEY private_key(EVP_PKCS82PKEY(p8inf.get()));
  if (!private_key.get())
    return Status::DataError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = ValidateKeyTypeAndCreateKeyAlgorithm(
      algorithm, private_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  // TODO(eroman): This is probably going to be the same as the input.
  std::vector<uint8_t> pkcs8_data;
  status = ExportPKeyPkcs8(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  scoped_ptr<AsymKeyOpenSsl> key_handle(
      new AsymKeyOpenSsl(private_key.Pass(), CryptoData(pkcs8_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePrivate,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);
  return Status::Success();
}

Status RsaHashedAlgorithm::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  crypto::ScopedBIO bio(BIO_new_mem_buf(const_cast<uint8_t*>(key_data.bytes()),
                                        key_data.byte_length()));
  if (!bio.get())
    return Status::ErrorUnexpected();

  crypto::ScopedEVP_PKEY public_key(d2i_PUBKEY_bio(bio.get(), NULL));
  if (!public_key.get())
    return Status::DataError();

  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = ValidateKeyTypeAndCreateKeyAlgorithm(
      algorithm, public_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  // TODO(eroman): This is probably going to be the same as the input.
  std::vector<uint8_t> spki_data;
  status = ExportPKeySpki(public_key.get(), &spki_data);
  if (status.IsError())
    return status;

  scoped_ptr<AsymKeyOpenSsl> key_handle(
      new AsymKeyOpenSsl(public_key.Pass(), CryptoData(spki_data)));

  *key = blink::WebCryptoKey::create(key_handle.release(),
                                     blink::WebCryptoKeyTypePublic,
                                     extractable,
                                     key_algorithm,
                                     usage_mask);
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeyPkcs8(const blink::WebCryptoKey& key,
                                          std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePrivate)
    return Status::ErrorUnexpectedKeyType();
  *buffer = AsymKeyOpenSsl::Cast(key)->serialized_key_data();
  return Status::Success();
}

Status RsaHashedAlgorithm::ExportKeySpki(const blink::WebCryptoKey& key,
                                         std::vector<uint8_t>* buffer) const {
  if (key.type() != blink::WebCryptoKeyTypePublic)
    return Status::ErrorUnexpectedKeyType();
  *buffer = AsymKeyOpenSsl::Cast(key)->serialized_key_data();
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
