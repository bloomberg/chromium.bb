// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nigori.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS)
#define MAYBE(name) name
#else
#define MAYBE(name) DISABLED_ ## name
#endif

TEST(NigoriTest, MAYBE(PermuteIsConstant)) {
  base::Nigori nigori1("example.com");
  EXPECT_TRUE(nigori1.Init("username", "password"));

  std::string permuted1;
  EXPECT_TRUE(nigori1.Permute(base::Nigori::Password, "name", &permuted1));

  base::Nigori nigori2("example.com");
  EXPECT_TRUE(nigori2.Init("username", "password"));

  std::string permuted2;
  EXPECT_TRUE(nigori2.Permute(base::Nigori::Password, "name", &permuted2));

  EXPECT_LT(0U, permuted1.size());
  EXPECT_EQ(permuted1, permuted2);
}

TEST(NigoriTest, MAYBE(EncryptDifferentIv)) {
  base::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("value");

  std::string encrypted1;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted1));

  std::string encrypted2;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted2));

  EXPECT_NE(encrypted1, encrypted2);
}

TEST(NigoriTest, MAYBE(EncryptDecrypt)) {
  base::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("value");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  std::string decrypted;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST(NigoriTest, MAYBE(CorruptedIv)) {
  base::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("test");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  // Corrupt the IV by changing one of its byte.
  encrypted[0] = (encrypted[0] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}

TEST(NigoriTest, MAYBE(CorruptedCiphertext)) {
  base::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("test");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  // Corrput the ciphertext by changing one of its bytes.
  encrypted[base::Nigori::kIvSize + 10] =
      (encrypted[base::Nigori::kIvSize + 10] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_FALSE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}
