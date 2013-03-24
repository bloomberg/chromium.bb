// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_encryptor.h"
#include "components/webdata/encryptor/encryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

const char kPlaintext[] = "The Magic Words are Squeamish Ossifrage";

class ChromeEncryptorTest : public testing::Test {
 protected:
  ChromeEncryptor encryptor_;
};

TEST_F(ChromeEncryptorTest, EncryptDecrypt) {
#if defined(OS_MACOSX)
  // ChromeEncryptor ends up needing access to the keychain on OS X,
  // so use the mock keychain to prevent prompts.
  ::Encryptor::UseMockKeychain(true);
#endif
  std::string ciphertext;
  EXPECT_TRUE(encryptor_.EncryptString(kPlaintext, &ciphertext));
  EXPECT_NE(kPlaintext, ciphertext);
  std::string plaintext;
  EXPECT_TRUE(encryptor_.DecryptString(ciphertext, &plaintext));
  EXPECT_EQ(kPlaintext, plaintext);
}

}  // namespace

}  // namespace browser_sync
