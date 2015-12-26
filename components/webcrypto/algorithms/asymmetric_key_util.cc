// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webcrypto/algorithms/asymmetric_key_util.h"

#include <openssl/pkcs12.h>
#include <stdint.h>
#include <utility>

#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"

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
      CreateAsymmetricKeyHandle(std::move(public_key), spki_data),
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
      CreateAsymmetricKeyHandle(std::move(private_key), pkcs8_data),
      blink::WebCryptoKeyTypePrivate, extractable, algorithm, usages);
  return Status::Success();
}

Status CheckPrivateKeyCreationUsages(
    blink::WebCryptoKeyUsageMask all_possible_usages,
    blink::WebCryptoKeyUsageMask actual_usages) {
  return CheckKeyCreationUsages(all_possible_usages, actual_usages,
                                EmptyUsagePolicy::REJECT_EMPTY);
}

Status CheckPublicKeyCreationUsages(
    blink::WebCryptoKeyUsageMask all_possible_usages,
    blink::WebCryptoKeyUsageMask actual_usages) {
  return CheckKeyCreationUsages(all_possible_usages, actual_usages,
                                EmptyUsagePolicy::ALLOW_EMPTY);
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

Status VerifyUsagesBeforeImportAsymmetricKey(
    blink::WebCryptoKeyFormat format,
    blink::WebCryptoKeyUsageMask all_public_key_usages,
    blink::WebCryptoKeyUsageMask all_private_key_usages,
    blink::WebCryptoKeyUsageMask usages) {
  switch (format) {
    case blink::WebCryptoKeyFormatSpki:
      return CheckPublicKeyCreationUsages(all_public_key_usages, usages);
    case blink::WebCryptoKeyFormatPkcs8:
      return CheckPrivateKeyCreationUsages(all_private_key_usages, usages);
    case blink::WebCryptoKeyFormatJwk: {
      // The JWK could represent either a public key or private key. The usages
      // must make sense for one of the two. The usages will be checked again by
      // ImportKeyJwk() once the key type has been determined.
      if (CheckPublicKeyCreationUsages(all_public_key_usages, usages)
              .IsError() &&
          CheckPrivateKeyCreationUsages(all_private_key_usages, usages)
              .IsError()) {
        return Status::ErrorCreateKeyBadUsages();
      }
      return Status::Success();
    }
    default:
      return Status::ErrorUnsupportedImportKeyFormat();
  }
}

Status GetUsagesForGenerateAsymmetricKey(
    blink::WebCryptoKeyUsageMask combined_usages,
    blink::WebCryptoKeyUsageMask all_public_usages,
    blink::WebCryptoKeyUsageMask all_private_usages,
    blink::WebCryptoKeyUsageMask* public_usages,
    blink::WebCryptoKeyUsageMask* private_usages) {
  // Ensure that the combined usages is a subset of the total possible usages.
  Status status =
      CheckKeyCreationUsages(all_public_usages | all_private_usages,
                             combined_usages, EmptyUsagePolicy::ALLOW_EMPTY);
  if (status.IsError())
    return status;

  *public_usages = combined_usages & all_public_usages;
  *private_usages = combined_usages & all_private_usages;

  // Ensure that the private key has non-empty usages.
  return CheckPrivateKeyCreationUsages(all_private_usages, *private_usages);
}

}  // namespace webcrypto
