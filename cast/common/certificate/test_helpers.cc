// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/test_helpers.h"

#include <openssl/pem.h>
#include <stdio.h>
#include <string.h>

namespace cast {
namespace certificate {
namespace testing {

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

}  // namespace testing
}  // namespace certificate
}  // namespace cast
