// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/pbkdf2.h"

namespace base {

SymmetricKey* DeriveKeyFromPassword(const std::string& password,
                                    const std::string& salt,
                                    unsigned int iterations,
                                    unsigned int key_size) {
  // TODO(albertb): Implement this on Mac.
  return NULL;
}

}  // namespace base
