// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/nigori.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NigoriTest, Parameters) {
  browser_sync::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));
  EXPECT_STREQ("example.com", nigori.hostname().c_str());
  EXPECT_STREQ("username", nigori.username().c_str());
  EXPECT_STREQ("password", nigori.password().c_str());
}

TEST(NigoriTest, Permute) {
  browser_sync::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string permuted;
  EXPECT_TRUE(nigori.Permute(browser_sync::Nigori::Password, "test name",
                             &permuted));

  std::string expected =
      "prewwdJj2PrGDczvmsHJEE5ndcCyVze8sY9kD5hjY/Tm"
      "c5kOjXFK7zB3Ss4LlHjEDirMu+vh85JwHOnGrMVe+g==";
  EXPECT_EQ(expected, permuted);
}

TEST(NigoriTest, PermuteIsConstant) {
  browser_sync::Nigori nigori1("example.com");
  EXPECT_TRUE(nigori1.Init("username", "password"));

  std::string permuted1;
  EXPECT_TRUE(nigori1.Permute(browser_sync::Nigori::Password,
                              "name",
                              &permuted1));

  browser_sync::Nigori nigori2("example.com");
  EXPECT_TRUE(nigori2.Init("username", "password"));

  std::string permuted2;
  EXPECT_TRUE(nigori2.Permute(browser_sync::Nigori::Password,
                              "name",
                              &permuted2));

  EXPECT_LT(0U, permuted1.size());
  EXPECT_EQ(permuted1, permuted2);
}

TEST(NigoriTest, EncryptDifferentIv) {
  browser_sync::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("value");

  std::string encrypted1;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted1));

  std::string encrypted2;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted2));

  EXPECT_NE(encrypted1, encrypted2);
}

TEST(NigoriTest, Decrypt) {
  browser_sync::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string encrypted =
      "e7+JyS6ibj6F5qqvpseukNRTZ+oBpu5iuv2VYjOfrH1dNiFLNf7Ov0"
      "kx/zicKFn0lJcbG1UmkNWqIuR4x+quDNVuLaZGbrJPhrJuj7cokCM=";

  std::string plaintext;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &plaintext));

  std::string expected("test, test, 1, 2, 3");
  EXPECT_EQ(expected, plaintext);
}

TEST(NigoriTest, EncryptDecrypt) {
  browser_sync::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("value");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  std::string decrypted;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST(NigoriTest, CorruptedIv) {
  browser_sync::Nigori nigori("example.com");
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

TEST(NigoriTest, CorruptedCiphertext) {
  browser_sync::Nigori nigori("example.com");
  EXPECT_TRUE(nigori.Init("username", "password"));

  std::string plaintext("test");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  // Corrput the ciphertext by changing one of its bytes.
  encrypted[browser_sync::Nigori::kIvSize + 10] =
      (encrypted[browser_sync::Nigori::kIvSize + 10] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_FALSE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}
