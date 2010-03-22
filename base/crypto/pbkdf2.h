// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_PBKDF2_H_
#define BASE_CRYPTO_PBKDF2_H_

#include <string>

#include "base/crypto/symmetric_key.h"

namespace base {

// Derives a key from the supplied password and salt using PBKDF2.
SymmetricKey* DeriveKeyFromPassword(const std::string& password,
                                    const std::string& salt,
                                    unsigned int iterations,
                                    unsigned int key_size);

}  // namespace base

#endif  // BASE_CRYPTO_PBKDF2_H_
