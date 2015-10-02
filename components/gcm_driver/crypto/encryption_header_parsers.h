// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_ENCRYPTION_HEADER_PARSERS_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_ENCRYPTION_HEADER_PARSERS_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace gcm {

// Structure representing the parsed values from the Encryption HTTP header.
// |salt| is stored after having been base64url decoded.
struct EncryptionHeaderValues {
  std::string keyid;
  std::string salt;
  uint64_t rs;
};

// Parses |input| following the syntax of the Encryption HTTP header. The parsed
// values will be stored in the |*values| argument.
//
// https://tools.ietf.org/html/draft-thomson-http-encryption-01#section-3
//
// This header follows the #list syntax from the extended ABNF syntax
// defined in section 1.2 of RFC 7230:
//
// https://tools.ietf.org/html/rfc7230#section-1.2
//
// Returns whether the |input| could be successfully parsed, and the resulting
// values are now available in the |*values| argument. Does not modify |*values|
// unless parsing was successful.
bool ParseEncryptionHeader(const std::string& input,
                           std::vector<EncryptionHeaderValues>* values);

// Structure representing the parsed values from the Encryption-Key HTTP header.
// |key| and |dh| are stored after having been base64url decoded.
struct EncryptionKeyHeaderValues {
  std::string keyid;
  std::string key;
  std::string dh;
};

// Parses |input| following the syntax of the Encryption-Key HTTP header. The
// parsed values will be stored in the |*values| argument.
//
// https://tools.ietf.org/html/draft-thomson-http-encryption-01#section-4
//
// This header follows the #list syntax from the extended ABNF syntax
// defined in section 1.2 of RFC 7230:
//
// https://tools.ietf.org/html/rfc7230#section-1.2
//
// Returns whether the |input| could be successfully parsed, and the resulting
// values are now available in the |*values| argument. Does not modify |*values|
// unless parsing was successful.
bool ParseEncryptionKeyHeader(const std::string& input,
                              std::vector<EncryptionKeyHeaderValues>* values);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_ENCRYPTION_HEADER_PARSERS_H_
