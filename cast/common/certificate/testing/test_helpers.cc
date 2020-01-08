// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/testing/test_helpers.h"

#include <openssl/bytestring.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <string.h>

#include "absl/strings/match.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace testing {

std::vector<std::string> ReadCertificatesFromPemFile(
    absl::string_view filename) {
  FILE* fp = fopen(filename.data(), "r");
  if (!fp) {
    return {};
  }
  std::vector<std::string> certs;
  char* name;
  char* header;
  unsigned char* data;
  long length;
  while (PEM_read(fp, &name, &header, &data, &length) == 1) {
    if (absl::StartsWith(name, "CERTIFICATE")) {
      certs.emplace_back((char*)data, length);
    }
    OPENSSL_free(name);
    OPENSSL_free(header);
    OPENSSL_free(data);
  }
  fclose(fp);
  return certs;
}

bssl::UniquePtr<EVP_PKEY> ReadKeyFromPemFile(absl::string_view filename) {
  FILE* fp = fopen(filename.data(), "r");
  if (!fp) {
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> pkey;
  char* name;
  char* header;
  unsigned char* data;
  long length;
  while (PEM_read(fp, &name, &header, &data, &length) == 1) {
    if (absl::StartsWith(name, "RSA PRIVATE KEY")) {
      OSP_DCHECK(!pkey);
      CBS cbs;
      CBS_init(&cbs, data, length);
      RSA* rsa = RSA_parse_private_key(&cbs);
      if (rsa) {
        pkey.reset(EVP_PKEY_new());
        EVP_PKEY_assign_RSA(pkey.get(), rsa);
      }
    }
    OPENSSL_free(name);
    OPENSSL_free(header);
    OPENSSL_free(data);
  }
  fclose(fp);
  return pkey;
}

SignatureTestData::SignatureTestData()
    : message{nullptr, 0}, sha1{nullptr, 0}, sha256{nullptr, 0} {}

SignatureTestData::~SignatureTestData() {
  OPENSSL_free(const_cast<uint8_t*>(message.data));
  OPENSSL_free(const_cast<uint8_t*>(sha1.data));
  OPENSSL_free(const_cast<uint8_t*>(sha256.data));
}

SignatureTestData ReadSignatureTestData(absl::string_view filename) {
  FILE* fp = fopen(filename.data(), "r");
  OSP_DCHECK(fp);
  SignatureTestData result = {};
  char* name;
  char* header;
  unsigned char* data;
  long length;
  while (PEM_read(fp, &name, &header, &data, &length) == 1) {
    if (strcmp(name, "MESSAGE") == 0) {
      OSP_DCHECK(!result.message.data);
      result.message.data = data;
      result.message.length = length;
    } else if (strcmp(name, "SIGNATURE SHA1") == 0) {
      OSP_DCHECK(!result.sha1.data);
      result.sha1.data = data;
      result.sha1.length = length;
    } else if (strcmp(name, "SIGNATURE SHA256") == 0) {
      OSP_DCHECK(!result.sha256.data);
      result.sha256.data = data;
      result.sha256.length = length;
    } else {
      OPENSSL_free(data);
    }
    OPENSSL_free(name);
    OPENSSL_free(header);
  }
  OSP_DCHECK(result.message.data);
  OSP_DCHECK(result.sha1.data);
  OSP_DCHECK(result.sha256.data);

  return result;
}

std::unique_ptr<TrustStore> CreateTrustStoreFromPemFile(
    absl::string_view filename) {
  std::unique_ptr<TrustStore> store = std::make_unique<TrustStore>();

  std::vector<std::string> certs =
      testing::ReadCertificatesFromPemFile(filename);
  for (const auto& der_cert : certs) {
    const uint8_t* data = (const uint8_t*)der_cert.data();
    store->certs.emplace_back(d2i_X509(nullptr, &data, der_cert.size()));
  }
  return store;
}

}  // namespace testing
}  // namespace cast
}  // namespace openscreen
