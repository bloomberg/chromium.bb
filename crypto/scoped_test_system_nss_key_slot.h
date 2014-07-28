// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_H_
#define CRYPTO_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/crypto_export.h"

namespace crypto {

class ScopedTestNSSDB;

// Opens a persistent NSS software database in a temporary directory and sets
// the test system slot to the opened database. This helper should be created in
// tests where no system token is provided by the Chaps module and before
// InitializeTPMTokenAndSystemSlot is called. Then the opened test database will
// be used and the initialization continues as if Chaps had provided this test
// database. In particular, the DB will be exposed by |GetSystemNSSKeySlot| and
// |IsTPMTokenReady| will return true.
// At most one instance of this helper must be used at a time.
class CRYPTO_EXPORT_PRIVATE ScopedTestSystemNSSKeySlot {
 public:
  explicit ScopedTestSystemNSSKeySlot();
  ~ScopedTestSystemNSSKeySlot();

  bool ConstructedSuccessfully() const;

 private:
  scoped_ptr<ScopedTestNSSDB> test_db_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestSystemNSSKeySlot);
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_TEST_SYSTEM_NSS_KEY_SLOT_H_
