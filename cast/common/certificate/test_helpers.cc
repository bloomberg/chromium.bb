// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/test_helpers.h"

#include <openssl/pem.h>
#include <stdio.h>
#include <string.h>

#include "platform/api/logging.h"

namespace cast {
namespace certificate {
namespace testing {

std::string ReadEntireFileToString(const std::string& filename) {
  FILE* file = fopen(filename.c_str(), "r");
  if (file == nullptr) {
    return {};
  }
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  std::string contents(file_size, 0);
  int bytes_read = 0;
  while (bytes_read < file_size) {
    size_t ret = fread(&contents[bytes_read], 1, file_size - bytes_read, file);
    if (ret == 0 && ferror(file)) {
      return {};
    } else {
      bytes_read += ret;
    }
  }
  fclose(file);

  return contents;
}

std::vector<std::string> ReadCertificatesFromPemFile(
    const std::string& filename) {
  FILE* fp = fopen(filename.c_str(), "r");
  if (!fp) {
    return {};
  }
  std::vector<std::string> certs;
#define STRCMP_LITERAL(s, l) strncmp(s, l, sizeof(l))
  for (;;) {
    char* name;
    char* header;
    unsigned char* data;
    long length;
    if (PEM_read(fp, &name, &header, &data, &length) == 1) {
      if (STRCMP_LITERAL(name, "CERTIFICATE") == 0) {
        certs.emplace_back((char*)data, length);
      }
      OPENSSL_free(name);
      OPENSSL_free(header);
      OPENSSL_free(data);
    } else {
      break;
    }
  }
  fclose(fp);
  return certs;
}

SignatureTestData::SignatureTestData()
    : message{nullptr, 0}, sha1{nullptr, 0}, sha256{nullptr, 0} {}

SignatureTestData::~SignatureTestData() {
  OPENSSL_free(const_cast<uint8_t*>(message.data));
  OPENSSL_free(const_cast<uint8_t*>(sha1.data));
  OPENSSL_free(const_cast<uint8_t*>(sha256.data));
}

SignatureTestData ReadSignatureTestData(const std::string& filename) {
  FILE* fp = fopen(filename.c_str(), "r");
  OSP_DCHECK(fp);
  SignatureTestData result = {};
  for (;;) {
    char* name;
    char* header;
    unsigned char* data;
    long length;
    if (PEM_read(fp, &name, &header, &data, &length) == 1) {
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
    } else {
      break;
    }
  }
  OSP_DCHECK(result.message.data);
  OSP_DCHECK(result.sha1.data);
  OSP_DCHECK(result.sha256.data);

  return result;
}

std::unique_ptr<TrustStore> CreateTrustStoreFromPemFile(
    const std::string& filename) {
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
}  // namespace certificate
}  // namespace cast
