// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/openssl/rsa_hashed_algorithm_openssl.h"

#include <openssl/evp.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/generate_key_result.h"
#include "content/child/webcrypto/jwk.h"
#include "content/child/webcrypto/openssl/key_openssl.h"
#include "content/child/webcrypto/openssl/util_openssl.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Creates a blink::WebCryptoAlgorithm having the modulus length and public
// exponent  of |key|.
Status CreateRsaHashedKeyAlgorithm(
    blink::WebCryptoAlgorithmId rsa_algorithm,
    blink::WebCryptoAlgorithmId hash_algorithm,
    EVP_PKEY* key,
    blink::WebCryptoKeyAlgorithm* key_algorithm) {
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

// Creates a WebCryptoKey that wraps |private_key|.
Status CreateWebCryptoRsaPrivateKey(
    crypto::ScopedEVP_PKEY private_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(
      rsa_algorithm_id, hash.id(), private_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  return CreateWebCryptoPrivateKey(private_key.Pass(), key_algorithm,
                                   extractable, usages, key);
}

// Creates a WebCryptoKey that wraps |public_key|.
Status CreateWebCryptoRsaPublicKey(
    crypto::ScopedEVP_PKEY public_key,
    const blink::WebCryptoAlgorithmId rsa_algorithm_id,
    const blink::WebCryptoAlgorithm& hash,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) {
  blink::WebCryptoKeyAlgorithm key_algorithm;
  Status status = CreateRsaHashedKeyAlgorithm(rsa_algorithm_id, hash.id(),
                                              public_key.get(), &key_algorithm);
  if (status.IsError())
    return status;

  return CreateWebCryptoPublicKey(public_key.Pass(), key_algorithm, extractable,
                                  usages, key);
}

Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usages,
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

  return CreateWebCryptoRsaPrivateKey(pkey.Pass(), algorithm.id(),
                                      algorithm.rsaHashedImportParams()->hash(),
                                      extractable, usages, key);
}

Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
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

  return CreateWebCryptoRsaPublicKey(pkey.Pass(), algorithm.id(),
                                     algorithm.rsaHashedImportParams()->hash(),
                                     extractable, usages, key);
}

}  // namespace

Status RsaHashedAlgorithm::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask combined_usages,
    GenerateKeyResult* result) const {
  blink::WebCryptoKeyUsageMask public_usages = 0;
  blink::WebCryptoKeyUsageMask private_usages = 0;

  Status status = GetUsagesForGenerateAsymmetricKey(
      combined_usages, all_public_key_usages_, all_private_key_usages_,
      &public_usages, &private_usages);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaHashedKeyGenParams* params =
      algorithm.rsaHashedKeyGenParams();

  unsigned int public_exponent = 0;
  unsigned int modulus_length_bits = 0;
  status =
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

  if (!RSA_generate_key_ex(rsa_private_key.get(), modulus_length_bits, bn.get(),
                           NULL)) {
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

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  // Note that extractable is unconditionally set to true. This is because per
  // the WebCrypto spec generated public keys are always extractable.
  status = CreateWebCryptoRsaPublicKey(public_pkey.Pass(), algorithm.id(),
                                       params->hash(), true, public_usages,
                                       &public_key);
  if (status.IsError())
    return status;

  status = CreateWebCryptoRsaPrivateKey(private_pkey.Pass(), algorithm.id(),
                                        params->hash(), extractable,
                                        private_usages, &private_key);
  if (status.IsError())
    return status;

  result->AssignKeyPair(public_key, private_key);
  return Status::Success();
}

Status RsaHashedAlgorithm::VerifyKeyUsagesBeforeImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask usages) const {
  return VerifyUsagesBeforeImportAsymmetricKey(format, all_public_key_usages_,
                                               all_private_key_usages_, usages);
}

Status RsaHashedAlgorithm::ImportKeyPkcs8(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::ScopedEVP_PKEY private_key;
  Status status =
      ImportUnverifiedPkeyFromPkcs8(key_data, EVP_PKEY_RSA, &private_key);
  if (status.IsError())
    return status;

  // Verify the parameters of the key.
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(private_key.get()));
  if (!rsa.get())
    return Status::ErrorUnexpected();
  if (!RSA_check_key(rsa.get()))
    return Status::DataError();

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoRsaPrivateKey(private_key.Pass(), algorithm.id(),
                                      algorithm.rsaHashedImportParams()->hash(),
                                      extractable, usages, key);
}

Status RsaHashedAlgorithm::ImportKeySpki(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::ScopedEVP_PKEY public_key;
  Status status =
      ImportUnverifiedPkeyFromSpki(key_data, EVP_PKEY_RSA, &public_key);
  if (status.IsError())
    return status;

  // TODO(eroman): Validate the algorithm OID against the webcrypto provided
  // hash. http://crbug.com/389400

  return CreateWebCryptoRsaPublicKey(public_key.Pass(), algorithm.id(),
                                     algorithm.rsaHashedImportParams()->hash(),
                                     extractable, usages, key);
}

Status RsaHashedAlgorithm::ImportKeyJwk(
    const CryptoData& key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key) const {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const char* jwk_algorithm =
      GetJwkAlgorithm(algorithm.rsaHashedImportParams()->hash().id());

  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  JwkRsaInfo jwk;
  Status status =
      ReadRsaKeyJwk(key_data, jwk_algorithm, extractable, usages, &jwk);
  if (status.IsError())
    return status;

  // Once the key type is known, verify the usages.
  status = CheckKeyCreationUsages(
      jwk.is_private_key ? all_private_key_usages_ : all_public_key_usages_,
      usages, !jwk.is_private_key);
  if (status.IsError())
    return status;

  return jwk.is_private_key
             ? ImportRsaPrivateKey(algorithm, extractable, usages, jwk, key)
             : ImportRsaPublicKey(algorithm, extractable, usages,
                                  CryptoData(jwk.n), CryptoData(jwk.e), key);
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
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  EVP_PKEY* pkey = AsymKeyOpenSsl::Cast(key)->key();
  crypto::ScopedRSA rsa(EVP_PKEY_get1_RSA(pkey));
  if (!rsa.get())
    return Status::ErrorUnexpected();

  const char* jwk_algorithm =
      GetJwkAlgorithm(key.algorithm().rsaHashedParams()->hash().id());
  if (!jwk_algorithm)
    return Status::ErrorUnexpected();

  switch (key.type()) {
    case blink::WebCryptoKeyTypePublic:
      WriteRsaPublicKeyJwk(CryptoData(BIGNUMToVector(rsa->n)),
                           CryptoData(BIGNUMToVector(rsa->e)), jwk_algorithm,
                           key.extractable(), key.usages(), buffer);
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
                            jwk_algorithm, key.extractable(), key.usages(),
                            buffer);
      return Status::Success();

    default:
      return Status::ErrorUnexpected();
  }
}

Status RsaHashedAlgorithm::SerializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<uint8_t>* key_data) const {
  key_data->assign(AsymKeyOpenSsl::Cast(key)->serialized_key_data());
  return Status::Success();
}

// TODO(eroman): Defer import to the crypto thread. http://crbug.com/430763
Status RsaHashedAlgorithm::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const CryptoData& key_data,
    blink::WebCryptoKey* key) const {
  blink::WebCryptoAlgorithm import_algorithm = CreateRsaHashedImportAlgorithm(
      algorithm.id(), algorithm.rsaHashedParams()->hash().id());

  Status status;

  switch (type) {
    case blink::WebCryptoKeyTypePublic:
      status =
          ImportKeySpki(key_data, import_algorithm, extractable, usages, key);
      break;
    case blink::WebCryptoKeyTypePrivate:
      status =
          ImportKeyPkcs8(key_data, import_algorithm, extractable, usages, key);
      break;
    default:
      return Status::ErrorUnexpected();
  }

  // There is some duplicated information in the serialized format used by
  // structured clone (since the KeyAlgorithm is serialized separately from the
  // key data). Use this extra information to further validate what was
  // deserialized from the key data.

  if (algorithm.id() != key->algorithm().id())
    return Status::ErrorUnexpected();

  if (key->type() != type)
    return Status::ErrorUnexpected();

  if (algorithm.rsaHashedParams()->modulusLengthBits() !=
      key->algorithm().rsaHashedParams()->modulusLengthBits()) {
    return Status::ErrorUnexpected();
  }

  if (algorithm.rsaHashedParams()->publicExponent().size() !=
          key->algorithm().rsaHashedParams()->publicExponent().size() ||
      0 !=
          memcmp(algorithm.rsaHashedParams()->publicExponent().data(),
                 key->algorithm().rsaHashedParams()->publicExponent().data(),
                 key->algorithm().rsaHashedParams()->publicExponent().size())) {
    return Status::ErrorUnexpected();
  }

  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
