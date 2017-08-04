// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/ec_private_key.h"
#include "crypto/rsa_private_key.h"
#include "net/base/hash_value.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/name_constraints.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/x509_certificate.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/stack.h"

namespace net {

namespace x509_util {

namespace {

bool GetCommonName(const der::Input& tlv, std::string* common_name) {
  RDNSequence rdn_sequence;
  if (!ParseName(tlv, &rdn_sequence))
    return false;

  for (const auto& rdn : rdn_sequence) {
    for (const auto& atv : rdn) {
      if (atv.type == TypeCommonNameOid()) {
        return atv.ValueAsString(common_name);
      }
    }
  }
  return true;
}

bool DecodeTime(const der::GeneralizedTime& generalized_time,
                base::Time* time) {
  base::Time::Exploded exploded = {0};
  exploded.year = generalized_time.year;
  exploded.month = generalized_time.month;
  exploded.day_of_month = generalized_time.day;
  exploded.hour = generalized_time.hours;
  exploded.minute = generalized_time.minutes;
  exploded.second = generalized_time.seconds;
  return base::Time::FromUTCExploded(exploded, time);
}

class BufferPoolSingleton {
 public:
  BufferPoolSingleton() : pool_(CRYPTO_BUFFER_POOL_new()) {}
  CRYPTO_BUFFER_POOL* pool() { return pool_; }

 private:
  // The singleton is leaky, so there is no need to use a smart pointer.
  CRYPTO_BUFFER_POOL* pool_;
};

base::LazyInstance<BufferPoolSingleton>::Leaky g_buffer_pool_singleton =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool GetTLSServerEndPointChannelBinding(const X509Certificate& certificate,
                                        std::string* token) {
  static const char kChannelBindingPrefix[] = "tls-server-end-point:";

  std::string der_encoded_certificate;
  if (!X509Certificate::GetDEREncoded(certificate.os_cert_handle(),
                                      &der_encoded_certificate))
    return false;

  der::Input tbs_certificate_tlv;
  der::Input signature_algorithm_tlv;
  der::BitString signature_value;
  if (!ParseCertificate(der::Input(&der_encoded_certificate),
                        &tbs_certificate_tlv, &signature_algorithm_tlv,
                        &signature_value, nullptr))
    return false;

  std::unique_ptr<SignatureAlgorithm> signature_algorithm =
      SignatureAlgorithm::Create(signature_algorithm_tlv, nullptr);
  if (!signature_algorithm)
    return false;

  const EVP_MD* digest_evp_md = nullptr;
  switch (signature_algorithm->digest()) {
    case net::DigestAlgorithm::Md2:
    case net::DigestAlgorithm::Md4:
      // Shouldn't be reachable.
      digest_evp_md = nullptr;
      break;

    // Per RFC 5929 section 4.1, MD5 and SHA1 map to SHA256.
    case net::DigestAlgorithm::Md5:
    case net::DigestAlgorithm::Sha1:
    case net::DigestAlgorithm::Sha256:
      digest_evp_md = EVP_sha256();
      break;

    case net::DigestAlgorithm::Sha384:
      digest_evp_md = EVP_sha384();
      break;

    case net::DigestAlgorithm::Sha512:
      digest_evp_md = EVP_sha512();
      break;
  }
  if (!digest_evp_md)
    return false;

  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned int out_size;
  if (!EVP_Digest(der_encoded_certificate.data(),
                  der_encoded_certificate.size(), digest, &out_size,
                  digest_evp_md, nullptr))
    return false;

  token->assign(kChannelBindingPrefix);
  token->append(digest, digest + out_size);
  return true;
}

// RSA keys created by CreateKeyAndSelfSignedCert will be of this length.
static const uint16_t kRSAKeyLength = 1024;

// Certificates made by CreateKeyAndSelfSignedCert and
//  CreateKeyAndChannelIDEC will be signed using this digest algorithm.
static const DigestAlgorithm kSignatureDigestAlgorithm = DIGEST_SHA256;

bool CreateKeyAndSelfSignedCert(const std::string& subject,
                                uint32_t serial_number,
                                base::Time not_valid_before,
                                base::Time not_valid_after,
                                std::unique_ptr<crypto::RSAPrivateKey>* key,
                                std::string* der_cert) {
  std::unique_ptr<crypto::RSAPrivateKey> new_key(
      crypto::RSAPrivateKey::Create(kRSAKeyLength));
  if (!new_key.get())
    return false;

  bool success = CreateSelfSignedCert(new_key.get(),
                                      kSignatureDigestAlgorithm,
                                      subject,
                                      serial_number,
                                      not_valid_before,
                                      not_valid_after,
                                      der_cert);
  if (success)
    *key = std::move(new_key);

  return success;
}

bool ParseCertificateSandboxed(const base::StringPiece& certificate,
                               std::string* subject,
                               std::string* issuer,
                               base::Time* not_before,
                               base::Time* not_after,
                               std::vector<std::string>* dns_names,
                               std::vector<std::string>* ip_addresses) {
  der::Input cert_data(certificate);
  der::Input tbs_cert, signature_alg;
  der::BitString signature_value;
  if (!ParseCertificate(cert_data, &tbs_cert, &signature_alg, &signature_value,
                        nullptr))
    return false;

  ParsedTbsCertificate parsed_tbs_cert;
  if (!ParseTbsCertificate(tbs_cert, ParseCertificateOptions(),
                           &parsed_tbs_cert, nullptr))
    return false;

  if (!GetCommonName(parsed_tbs_cert.subject_tlv, subject))
    return false;

  if (!GetCommonName(parsed_tbs_cert.issuer_tlv, issuer))
    return false;

  if (!DecodeTime(parsed_tbs_cert.validity_not_before, not_before))
    return false;

  if (!DecodeTime(parsed_tbs_cert.validity_not_after, not_after))
    return false;

  if (!parsed_tbs_cert.has_extensions)
    return true;

  std::map<der::Input, ParsedExtension> extensions;
  if (!ParseExtensions(parsed_tbs_cert.extensions_tlv, &extensions))
    return false;

  CertErrors unused_errors;
  std::vector<std::string> san;
  auto iter = extensions.find(SubjectAltNameOid());
  if (iter != extensions.end()) {
    std::unique_ptr<GeneralNames> subject_alt_names =
        GeneralNames::Create(iter->second.value, &unused_errors);
    if (subject_alt_names) {
      *dns_names = subject_alt_names->dns_names;
      for (const auto& ip : subject_alt_names->ip_addresses)
        ip_addresses->push_back(ip.ToString());
    }
  }

  return true;
}

CRYPTO_BUFFER_POOL* GetBufferPool() {
  return g_buffer_pool_singleton.Get().pool();
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(const uint8_t* data,
                                                  size_t length) {
  return bssl::UniquePtr<CRYPTO_BUFFER>(
      CRYPTO_BUFFER_new(data, length, GetBufferPool()));
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    const base::StringPiece& data) {
  return bssl::UniquePtr<CRYPTO_BUFFER>(
      CRYPTO_BUFFER_new(reinterpret_cast<const uint8_t*>(data.data()),
                        data.size(), GetBufferPool()));
}

scoped_refptr<X509Certificate> CreateX509CertificateFromBuffers(
    STACK_OF(CRYPTO_BUFFER) * buffers) {
  if (sk_CRYPTO_BUFFER_num(buffers) == 0) {
    NOTREACHED();
    return nullptr;
  }

#if BUILDFLAG(USE_BYTE_CERTS)
  std::vector<CRYPTO_BUFFER*> intermediate_chain;
  for (size_t i = 1; i < sk_CRYPTO_BUFFER_num(buffers); ++i)
    intermediate_chain.push_back(sk_CRYPTO_BUFFER_value(buffers, i));
  return X509Certificate::CreateFromHandle(sk_CRYPTO_BUFFER_value(buffers, 0),
                                           intermediate_chain);
#else
  // Convert the certificate chains to a platform certificate handle.
  std::vector<base::StringPiece> der_chain;
  der_chain.reserve(sk_CRYPTO_BUFFER_num(buffers));
  for (size_t i = 0; i < sk_CRYPTO_BUFFER_num(buffers); ++i) {
    const CRYPTO_BUFFER* cert = sk_CRYPTO_BUFFER_value(buffers, i);
    der_chain.push_back(base::StringPiece(
        reinterpret_cast<const char*>(CRYPTO_BUFFER_data(cert)),
        CRYPTO_BUFFER_len(cert)));
  }
  return X509Certificate::CreateFromDERCertChain(der_chain);
#endif
}

}  // namespace x509_util

}  // namespace net
