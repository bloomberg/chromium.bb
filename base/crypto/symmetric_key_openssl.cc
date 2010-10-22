// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/symmetric_key.h"

#include "base/logging.h"

namespace base {

SymmetricKey::~SymmetricKey() {
}

// static
SymmetricKey* SymmetricKey::GenerateRandomKey(Algorithm algorithm,
                                              size_t key_size_in_bits) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
SymmetricKey* SymmetricKey::DeriveKeyFromPassword(Algorithm algorithm,
                                                  const std::string& password,
                                                  const std::string& salt,
                                                  size_t iterations,
                                                  size_t key_size_in_bits) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
SymmetricKey* SymmetricKey::Import(Algorithm algorithm,
                                   const std::string& raw_key) {
  NOTIMPLEMENTED();
  return NULL;
}

bool SymmetricKey::GetRawKey(std::string* raw_key) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
