// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_

#include <string>

#include "base/strings/string_piece_forward.h"

namespace password_manager {

// Username hash prefix length in bits.
constexpr size_t kUsernameHashPrefixLength = 24;

// Canonicalizes |username| by lower-casing and and stripping a mail-address
// host in case the username is a mail address. |username| must be a UTF-8
// string.
std::string CanonicalizeUsername(base::StringPiece username);

// Hashes |canonicalized_username| by appending a fixed salt and computing the
// SHA256 hash.
std::string HashUsername(base::StringPiece canonicalized_username);

// Bucketizes |canonicalized_username| by hashing it and returning a prefix of
// 24 bits.
std::string BucketizeUsername(base::StringPiece canonicalized_username);

// Produces the username/password pair hash using scrypt algorithm.
// |username| and |password| are UTF-8 strings.
std::string ScryptHashUsernameAndPassword(base::StringPiece username,
                                          base::StringPiece password);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
