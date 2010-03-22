// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRYPTO_SYMMETRIC_KEY_H_
#define BASE_CRYPTO_SYMMETRIC_KEY_H_

#include <string>

#include "base/basictypes.h"

#if defined(USE_NSS)
#include "base/crypto/scoped_nss_types.h"
#endif  // USE_NSS

namespace base {

// Wraps a platform-specific symmetric key and allows it to be held in a
// scoped_ptr.
class SymmetricKey {
 public:
#if defined(USE_NSS)
  explicit SymmetricKey(PK11SymKey* key) : key_(key) {}
#endif  // USE_NSS

  virtual ~SymmetricKey() {}

#if defined(USE_NSS)
  PK11SymKey* key() const { return key_.get(); }
#endif  // USE_NSS

  // Extracts the raw key from the platform specific data. This should only be
  // done in unit tests to verify that keys are generated correctly.
  bool GetRawKey(std::string* raw_key);

 private:
#if defined(USE_NSS)
  ScopedPK11SymKey key_;
#endif  // USE_NSS

  DISALLOW_COPY_AND_ASSIGN(SymmetricKey);
};

}  // namespace base

#endif   // BASE_CRYPTO_SYMMETRIC_KEY_H_
