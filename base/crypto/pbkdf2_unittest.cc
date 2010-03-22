// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/pbkdf2.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestVector {
  const char* password;
  const char* salt;
  unsigned int rounds;
  unsigned int key_size;
  const char* expected;
};

// These are the test vectors suggested in:
// http://www.ietf.org/id/draft-josefsson-pbkdf2-test-vectors-00.txt
static const TestVector test_vectors[] = {
  {
    "password",
    "salt",
    1,
    20,
    "\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9"
    "\xb5\x24\xaf\x60\x12\x06\x2f\xe0\x37\xa6",
  },
  {
    "password",
    "salt",
    2,
    20,
    "\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e"
    "\xd9\x2a\xce\x1d\x41\xf0\xd8\xde\x89\x57",
  },
  {
    "password",
    "salt",
    4096,
    20,
    "\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad"
    "\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1",
  },
  // This test takes over 30s to run on the trybots.
#if 0
  {
    "password",
    "salt",
    16777216,
    20,
    "\xee\xfe\x3d\x61\xcd\x4d\xa4\xe4\xe9\x94"
    "\x5b\x3d\x6b\xa2\x15\x8c\x26\x34\xe9\x84",
  },
#endif
};

#if defined(USE_NSS)
#define MAYBE_TestVectors TestVectors
#else
#define MAYBE_TestVectors DISABLED_TestVectors
#endif
TEST(PBKDF2Test, MAYBE_TestVectors) {
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_vectors); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%u]", i));
    scoped_ptr<base::SymmetricKey> key(base::DeriveKeyFromPassword(
        test_vectors[i].password, test_vectors[i].salt, test_vectors[i].rounds,
        test_vectors[i].key_size));
    EXPECT_TRUE(NULL != key.get());

    std::string raw_key;
    key->GetRawKey(&raw_key);
    EXPECT_EQ(test_vectors[i].key_size, raw_key.size());
    EXPECT_STREQ(test_vectors[i].expected, raw_key.c_str());
  }
}
