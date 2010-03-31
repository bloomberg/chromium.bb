// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/encryptor.h"

#include <string>

#include "base/crypto/symmetric_key.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS) || defined(OS_MACOSX)
#define MAYBE(name) name
#else
#define MAYBE(name) DISABLED_ ## name
#endif

TEST(EncryptorTest, MAYBE(EncryptDecrypt)) {
  scoped_ptr<base::SymmetricKey> key(base::SymmetricKey::DeriveKeyFromPassword(
      base::SymmetricKey::AES, "password", "saltiest", 1000, 256));
  EXPECT_TRUE(NULL != key.get());

  base::Encryptor encryptor;
  // The IV must be exactly as long a the cipher block size.
  std::string iv("the iv: 16 bytes");
  EXPECT_TRUE(encryptor.Init(key.get(), base::Encryptor::CBC, iv));

  std::string plaintext("this is the plaintext");
  std::string ciphertext;
  EXPECT_TRUE(encryptor.Encrypt(plaintext, &ciphertext));

  EXPECT_LT(0U, ciphertext.size());

  std::string decypted;
  EXPECT_TRUE(encryptor.Decrypt(ciphertext, &decypted));

  EXPECT_EQ(plaintext, decypted);
}
