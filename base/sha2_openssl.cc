// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sha2.h"

#include <openssl/ssl.h>

#include "base/basictypes.h"
#include "base/openssl_util.h"
#include "base/stl_util-inl.h"

namespace base {

void SHA256HashString(const std::string& str, void* output, size_t len) {
  COMPILE_ASSERT(SHA256_LENGTH == SHA256_DIGEST_LENGTH,
                 API_and_OpenSSL_SHA256_lengths_must_match);
  ScopedOpenSSLSafeSizeBuffer<SHA256_DIGEST_LENGTH> result(
      reinterpret_cast<unsigned char*>(output), len);
  ::SHA256(reinterpret_cast<const unsigned char*>(str.data()), str.size(),
           result.safe_buffer());
}

std::string SHA256HashString(const std::string& str) {
  std::string output(SHA256_LENGTH, 0);
  SHA256HashString(str, string_as_array(&output), output.size());
  return output;
}

}  // namespace base
