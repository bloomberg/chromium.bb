// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/symmetric_key.h"

namespace base {

// TODO(albertb): Implement on Windows.

// static
SymmetricKey* SymmetricKey::GenerateRandomKey(Algorithm algorithm, unsigned int key_size) {
  return NULL;
}

// static
SymmetricKey* SymmetricKey::DeriveKeyFromPassword(Algorithm algorithm,
                                                  const std::string& password,
                                                  const std::string& salt,
                                                  size_t iterations,
                                                  size_t key_size) {
  return NULL;
}

bool SymmetricKey::GetRawKey(std::string* raw_key) {
  return false;
}

}  // namespace base
