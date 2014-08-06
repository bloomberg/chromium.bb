// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/rsa_key_openssl.h"

#include <openssl/evp.h>
#include <openssl/pkcs12.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/jwk.h"
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

Status CreateWebCryptoPrivateKey(
    crypto::ScopedEVP_PKEY private_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(
      rsa_algorithm_id, hash.id(), private_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  // Serialize the key at creation time so that if structured cloning is
  // requested it can be done synchronously from the Blink thread.
  std::vector<uint8_t> pkcs8_data;
  status = ExportPKeyPkcs8(private_key.get(), &pkcs8_data);
  if (status.IsError())
    return status;

  *key = blink::WebCryptoKey::create(
      new AsymKeyOpenSsl(private_key.Pass(), CryptoData(pkcs8_data)),
      blink::WebCryptoKeyTypePrivate,
      extractable,
      key_algorithm,
      usage_mask);
  return Status::Success();
}

Status CreateWebCryptoPublicKey(
    crypto::ScopedEVP_PKEY public_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(
      rsa_algorithm_id, hash.id(), public_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  // Serialize the key at creation time so that if structured cloning is
  // requested it can be done synchronously from the Blink thread.
  std::vector<uint8_t> spki_data;
  status = ExportPKeySpki(public_key.get(), &spki_data);
  if (status.IsError())
    return status;

  *key = blink::WebCryptoKey::create(
      new AsymKeyOpenSsl(public_key.Pass(), CryptoData(spki_data)),
      blink::WebCryptoKeyTypePublic,
      extractable,
      key_algorithm,
      usage_mask);
  return Status::Success();
}

// Converts a BIGNUM to a big endian byte array.
std::vector<uint8_t> BIGNUMToVector(BIGNUM* n) {
  std::vector<uint8_t> v(BN_num_bytes(n));
  BN_bn2bin(n, vector_as_array(&v));
  return v;
}

// Allocates a new BIGNUM given a std::string big-endian representation.
BIGNUM* CreateBIGNUM(const std::string& n) {
  return BN_bin2bn(reinterpret_cast<const uint8_t*>(n.data()), n.size(), NULL);
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           const JwkRsaInfo& params,
                           blink::WebCryptoKey* key) {
  crypto::ScopedRSA rsa(RSA_new());

  rsa->n = CreateBIGNUM(params.n);
  rsa->e = CreateBIGNUM(params.e);
  rsa->d = CreateBIGNUM(params.d);
  rsa->p = CreateBIGNUM(params.p);
  rsa->q = CreateBIGNUM(params.q);
  rsa->dmp1 = CreateBIGNUM(params.dp);
  rsa->dmq1 = CreateBIGNUM(params.dq);
  rsa->iqmp = CreateBIGNUM(params.qi);

  if (!rsa->n || !rsa->e || !rsa->d || !rsa->p || !rsa->q || !rsa->dmp1 ||
      !rsa->dmq1 || !rsa->iqmp) {
    return Status::OperationError();
  }

  // TODO(eroman): This should really be a DataError, however for compatibility
  //               with NSS it is an OperationError.
  if (!RSA_check_key(rsa.get()))
    return Status::OperationError();

  // Create a corresponding EVP_PKEY.
  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return Status::OperationError();

  return CreateWebCryptoPrivateKey(pkey.Pass(),
                                   algorithm.id(),
                                   algorithm.rsaHashedImportParams()->hash(),
                                   extractable,
                                   usage_mask,
                                   key);
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          const CryptoData& n,
                          const CryptoData& e,
                          blink::WebCryptoKey* key) {
  crypto::ScopedRSA rsa(RSA_new());

  rsa->n = BN_bin2bn(n.bytes(), n.byte_length(), NULL);
  rsa->e = BN_bin2bn(e.bytes(), e.byte_length(), NULL);

  if (!rsa->n || !rsa->e)
    return Status::OperationError();

  // Create a corresponding EVP_PKEY.
  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!pkey || !EVP_PKEY_set1_RSA(pkey.get(), rsa.get()))
    return Status::OperationError();

  return CreateWebCryptoPublicKey(pkey.Pass(),
                                  algorithm.id(),
                                  algorithm.rsaHashedImportParams()->hash(),
                                  extractable,
                                  usage_mask,
                                  key);
}

}  // namespace

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeGenerateKeyPair(
    blink::WebCryptoKeyUsageMask combined_usage_mask,
    blink::WebCryptoKeyUsageMask* public_usage_mask,
    blink::WebCryptoKeyUsageMask* private_usage_mask) const {
  Status status = CheckKeyCreationUsages(
      all_public_key_usages_ | all_private_key_usages_, combined_usage_mask);
  if (status.IsError())
    return status;

  *public_usage_mask = combined_usage_mask & all_public_key_usages_;
  *private_usage_mask = combined_usage_mask & all_private_key_usages_;

  return Status::Success();
}

Status RsaHashedAlgorithm::GenerateKeyPair(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask public_usage_mask,
    blink::WebCryptoKeyUsageMask private_usage_mask,
    blink::WebCryptoKey* public_key,
    blink::WebCryptoKey* private_key) const {
  const blink::WebCryptoRsaHashedKeyGenParams* params =
      algorithm.rsaHashedKeyGenParams();

  unsigned int public_exponent = 0;
  unsigned int modulus_length_bits = 0;
  Status status =
      GetRsaKeyGenParameters(params, &public_exponent, &modulus_length_bits);
  if (status.IsError())
    return status;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Generate an RSA key pair.
  crypto::ScopedRSA rsa_private_key(RSA_new());
  crypto::ScopedBIGNUM bn(BN_new());
  if (!rsa_private_key.get() || !bn.get() ||
      !BN_set_word(bn.get(), public_exponent)) {
    return Status::OperationError();
  }

  if (!RSA_generate_key_ex(
          rsa_private_key.get(), modulus_length_bits, bn.get(), NULL)) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for the private key.
  crypto::ScopedEVP_PKEY private_pkey(EVP_PKEY_new());
  if (!private_pkey ||
      !EVP_PKEY_set1_RSA(private_pkey.get(), rsa_private_key.get())) {
    return Status::OperationError();
  }

  // Construct an EVP_PKEY for the public key.
  crypto::ScopedRSA rsa_public_key(RSAPublicKey_dup(rsa_private_key.get()));
  crypto::ScopedEVP_PKEY public_pkey(EVP_PKEY_new());
  if (!public_pkey ||
      !EVP_PKEY_set1_RSA(public_pkey.get(), rsa_public_key.get())) {
    return Status::OperationError();
  }

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always public.
  status = CreateWebCryptoPublicKey(public_pkey.Pass(),
                                    algorithm.id(),
                                    params->hash(),
                                    true,
                                    public_usage_mask,
                                    public_key);
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(private_pkey.Pass(),
                                   algorithm.id(),
                                   params->hash(),
                                   extractable,
                                   private_usage_mask,
                                   private_key);
}

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usage_mask) const {
  switch (format) {
    case blink::WebCryptoKeyFormatSpki:
      return CheckKeyCreationUsages(all_public_key_usages_, usage_mask);
    case blink::WebCryptoKeyFormatPkcs8:
      return CheckKeyCreationUsages(all_private_key_usages_, usage_mask);
    case blink::WebCryptoKeyFormatJwk:
      // TODO(eroman): http://crbug.com/395904
      return CheckKeyCreationUsages(
          all_public_key_usages_ | all_private_key_usages_, usage_mask);
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

  if (EVP_PKEY_id(private_key.get()) != EVP_PKEY_RSA)
    return Status::DataError();  // Data did not define an RSA key.

  // Verify the parameters of the key (because EVP_PKCS82PKEY() happily imports
  // invalid keys).
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(private_key.get()));
  if (!rsa.get())
    return Status::ErrorUnexpected();
  if (!RSA_check_key(rsa.get()))
    return Status::DataError();

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoPrivateKey(private_key.Pass(),
                                   algorithm.id(),
                                   algorithm.rsaHashedImportParams()->hash(),
                                   extractable,
                                   usage_mask,
                                   key);
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

  if (EVP_PKEY_id(public_key.get()) != EVP_PKEY_RSA)
    return Status::DataError();  // Data did not define an RSA key.

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoPublicKey(public_key.Pass(),
                                  algorithm.id(),
                                  algorithm.rsaHashedImportParams()->hash(),
                                  extractable,
                                  usage_mask,
                                  key);
}

Status RsaHashedAlgorithm::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) const {
  const char* jwk_algorithm =
      GetJwkAlgorithm(algorithm.rsaHashedImportParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  JwkRsaInfo jwk;
  Status status =
      ReadRsaKeyJwk(key_data, jwk_algorithm, extractable, usage_mask, &jwk);
  if (status.IsError())
    return status;

  // Once the key type is known, verify the usages.
  status = CheckKeyCreationUsages(
      jwk.is_private_key ? all_private_key_usages_ : all_public_key_usages_,
      usage_mask);
  if (status.IsError())
    return status;

  return jwk.is_private_key
             ? ImportRsaPrivateKey(algorithm, extractable, usage_mask, jwk, key)
             : ImportRsaPublicKey(algorithm,
                                  extractable,
                                  usage_mask,
                                  CryptoData(jwk.n),
                                  CryptoData(jwk.e),
                                  key);
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

Status RsaHashedAlgorithm::ExportKeyJwk(const blink::WebCryptoKey& key,
                                        std::vector<uint8_t>* buffer) const {
  EVP_PKEY* public_key = AsymKeyOpenSsl::Cast(key)->key();
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(public_key));
  if (!rsa.get())
    return Status::ErrorUnexpected();

  const char* jwk_algorithm =
      GetJwkAlgorithm(key.algorithm().rsaHashedParams()->hash().id());
  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  switch (key.type()) {
    case blink::WebCryptoKeyTypePublic:
      WriteRsaPublicKeyJwk(CryptoData(BIGNUMToVector(rsa->n)),
                           CryptoData(BIGNUMToVector(rsa->e)),
                           jwk_algorithm,
                           key.extractable(),
                           key.usages(),
                           buffer);
      return Status::Success();
    case blink::WebCryptoKeyTypePrivate:
      WriteRsaPrivateKeyJwk(CryptoData(BIGNUMToVector(rsa->n)),
                            CryptoData(BIGNUMToVector(rsa->e)),
                            CryptoData(BIGNUMToVector(rsa->d)),
                            CryptoData(BIGNUMToVector(rsa->p)),
                            CryptoData(BIGNUMToVector(rsa->q)),
                            CryptoData(BIGNUMToVector(rsa->dmp1)),
                            CryptoData(BIGNUMToVector(rsa->dmq1)),
                            CryptoData(BIGNUMToVector(rsa->iqmp)),
                            jwk_algorithm,
                            key.extractable(),
                            key.usages(),
                            buffer);
      return Status::Success();

    default:
      return Status::ErrorUnexpected();
  }
}

}  // namespace webcrypto

}  // namespace content
