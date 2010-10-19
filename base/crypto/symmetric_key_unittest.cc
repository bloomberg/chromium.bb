// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/symmetric_key.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SymmetricKeyTest, GenerateRandomKey) {
  scoped_ptr<base::SymmetricKey> key(
      base::SymmetricKey::GenerateRandomKey(base::SymmetricKey::AES, 256));
  ASSERT_TRUE(NULL != key.get());
  std::string raw_key;
  EXPECT_TRUE(key->GetRawKey(&raw_key));
  EXPECT_EQ(32U, raw_key.size());

  // Do it again and check that the keys are different.
  // (Note: this has a one-in-10^77 chance of failure!)
  scoped_ptr<base::SymmetricKey> key2(
      base::SymmetricKey::GenerateRandomKey(base::SymmetricKey::AES, 256));
  ASSERT_TRUE(NULL != key2.get());
  std::string raw_key2;
  EXPECT_TRUE(key2->GetRawKey(&raw_key2));
  EXPECT_EQ(32U, raw_key2.size());
  EXPECT_NE(raw_key, raw_key2);
}

TEST(SymmetricKeyTest, ImportGeneratedKey) {
  scoped_ptr<base::SymmetricKey> key1(
      base::SymmetricKey::GenerateRandomKey(base::SymmetricKey::AES, 256));
  ASSERT_TRUE(NULL != key1.get());
  std::string raw_key1;
  EXPECT_TRUE(key1->GetRawKey(&raw_key1));

  scoped_ptr<base::SymmetricKey> key2(
      base::SymmetricKey::Import(base::SymmetricKey::AES, raw_key1));
  ASSERT_TRUE(NULL != key2.get());

  std::string raw_key2;
  EXPECT_TRUE(key2->GetRawKey(&raw_key2));

  EXPECT_EQ(raw_key1, raw_key2);
}

TEST(SymmetricKeyTest, ImportDerivedKey) {
  scoped_ptr<base::SymmetricKey> key1(
      base::SymmetricKey::DeriveKeyFromPassword(base::SymmetricKey::HMAC_SHA1,
                                                "password", "somesalt", 1024,
                                                160));
  ASSERT_TRUE(NULL != key1.get());
  std::string raw_key1;
  EXPECT_TRUE(key1->GetRawKey(&raw_key1));

  scoped_ptr<base::SymmetricKey> key2(
      base::SymmetricKey::Import(base::SymmetricKey::HMAC_SHA1, raw_key1));
  ASSERT_TRUE(NULL != key2.get());

  std::string raw_key2;
  EXPECT_TRUE(key2->GetRawKey(&raw_key2));

  EXPECT_EQ(raw_key1, raw_key2);
}

struct PBKDF2TestVector {
  const char* password;
  const char* salt;
  unsigned int rounds;
  unsigned int key_size_in_bits;
  const uint8 expected[21];  // string literals need 1 extra NUL byte
};

static const PBKDF2TestVector test_vectors[] = {
  // These tests come from
  // http://www.ietf.org/id/draft-josefsson-pbkdf2-test-vectors-00.txt
  {
    "password",
    "salt",
    1,
    160,
    "\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9"
    "\xb5\x24\xaf\x60\x12\x06\x2f\xe0\x37\xa6",
  },
  {
    "password",
    "salt",
    2,
    160,
    "\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e"
    "\xd9\x2a\xce\x1d\x41\xf0\xd8\xde\x89\x57",
  },
  {
    "password",
    "salt",
    4096,
    160,
    "\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad"
    "\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1",
  },
  // This test takes over 30s to run on the trybots.
#if 0
  {
    "password",
    "salt",
    16777216,
    160,
    "\xee\xfe\x3d\x61\xcd\x4d\xa4\xe4\xe9\x94"
    "\x5b\x3d\x6b\xa2\x15\x8c\x26\x34\xe9\x84",
  },
#endif

  // These tests come from RFC 3962, via BSD source code at
  // http://www.openbsd.org/cgi-bin/cvsweb/src/sbin/bioctl/pbkdf2.c?rev=HEAD&content-type=text/plain
  {
    "password",
    "ATHENA.MIT.EDUraeburn",
    1,
    160,
    {
      0xcd, 0xed, 0xb5, 0x28, 0x1b, 0xb2, 0xf8, 0x01,
      0x56, 0x5a, 0x11, 0x22, 0xb2, 0x56, 0x35, 0x15,
      0x0a, 0xd1, 0xf7, 0xa0
    },
  },
  {
    "password",
    "ATHENA.MIT.EDUraeburn",
    2,
    160,
    {
      0x01, 0xdb, 0xee, 0x7f, 0x4a, 0x9e, 0x24, 0x3e,
      0x98, 0x8b, 0x62, 0xc7, 0x3c, 0xda, 0x93, 0x5d,
      0xa0, 0x53, 0x78, 0xb9
    },
  },
  {
    "password",
    "ATHENA.MIT.EDUraeburn",
    1200,
    160,
    {
      0x5c, 0x08, 0xeb, 0x61, 0xfd, 0xf7, 0x1e, 0x4e,
      0x4e, 0xc3, 0xcf, 0x6b, 0xa1, 0xf5, 0x51, 0x2b,
      0xa7, 0xe5, 0x2d, 0xdb
    },
  },
  {
    "password",
    "\0224VxxV4\022", /* 0x1234567878563412 */
    5,
    160,
    {
      0xd1, 0xda, 0xa7, 0x86, 0x15, 0xf2, 0x87, 0xe6,
      0xa1, 0xc8, 0xb1, 0x20, 0xd7, 0x06, 0x2a, 0x49,
      0x3f, 0x98, 0xd2, 0x03
    },
  },
  {
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
    "pass phrase equals block size",
    1200,
    160,
    {
      0x13, 0x9c, 0x30, 0xc0, 0x96, 0x6b, 0xc3, 0x2b,
      0xa5, 0x5f, 0xdb, 0xf2, 0x12, 0x53, 0x0a, 0xc9,
      0xc5, 0xec, 0x59, 0xf1
    },
  },
  {
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
    "pass phrase exceeds block size",
    1200,
    160,
    {
      0x9c, 0xca, 0xd6, 0xd4, 0x68, 0x77, 0x0c, 0xd5,
      0x1b, 0x10, 0xe6, 0xa6, 0x87, 0x21, 0xbe, 0x61,
      0x1a, 0x8b, 0x4d, 0x28
    },
  },
  {
    "\360\235\204\236", /* g-clef (0xf09d849e) */
    "EXAMPLE.COMpianist",
    50,
    160,
    {
      0x6b, 0x9c, 0xf2, 0x6d, 0x45, 0x45, 0x5a, 0x43,
      0xa5, 0xb8, 0xbb, 0x27, 0x6a, 0x40, 0x3b, 0x39,
      0xe7, 0xfe, 0x37, 0xa0
    },
  }
};

TEST(SymmetricKeyTest, DeriveKeyFromPassword) {
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_vectors); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%u]", i));
#if defined(OS_MACOSX)
    // The OS X crypto libraries have minimum salt and iteration requirements
    // so some of the above tests will cause them to barf. Skip these.
    if (strlen(test_vectors[i].salt) < 8 || test_vectors[i].rounds < 1000) {
      VLOG(1) << "Skipped test vector #" << i;
      continue;
    }
#endif  // OS_MACOSX
    scoped_ptr<base::SymmetricKey> key(
        base::SymmetricKey::DeriveKeyFromPassword(
            base::SymmetricKey::HMAC_SHA1,
            test_vectors[i].password, test_vectors[i].salt,
            test_vectors[i].rounds, test_vectors[i].key_size_in_bits));
    ASSERT_TRUE(NULL != key.get());

    std::string raw_key;
    key->GetRawKey(&raw_key);
    EXPECT_EQ(test_vectors[i].key_size_in_bits / 8, raw_key.size());
    EXPECT_EQ(0, memcmp(test_vectors[i].expected,
                        raw_key.data(),
                        raw_key.size()));
  }
}
