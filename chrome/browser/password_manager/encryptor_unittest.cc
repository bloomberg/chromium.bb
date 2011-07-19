// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/encryptor.h"

#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class EncryptorTest : public testing::Test {
 public:
  EncryptorTest() {}

  virtual void SetUp() {
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EncryptorTest);
};

TEST_F(EncryptorTest, String16EncryptionDecryption) {
  string16 plaintext;
  string16 result;
  std::string utf8_plaintext;
  std::string utf8_result;
  std::string ciphertext;

  // Test borderline cases (empty strings).
  EXPECT_TRUE(Encryptor::EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test a simple string.
  plaintext = ASCIIToUTF16("hello");
  EXPECT_TRUE(Encryptor::EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test a 16-byte aligned string.  This previously hit a boundary error in
  // base::Encryptor::Crypt() on Mac.
  plaintext = ASCIIToUTF16("1234567890123456");
  EXPECT_TRUE(Encryptor::EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test Unicode.
  char16 wchars[] = { 0xdbeb, 0xdf1b, 0x4e03, 0x6708, 0x8849,
                      0x661f, 0x671f, 0x56db, 0x597c, 0x4e03,
                      0x6708, 0x56db, 0x6708, 0xe407, 0xdbaf,
                      0xdeb5, 0x4ec5, 0x544b, 0x661f, 0x671f,
                      0x65e5, 0x661f, 0x671f, 0x4e94, 0xd8b1,
                      0xdce1, 0x7052, 0x5095, 0x7c0b, 0xe586, 0};
  plaintext = wchars;
  utf8_plaintext = UTF16ToUTF8(plaintext);
  EXPECT_EQ(plaintext, UTF8ToUTF16(utf8_plaintext));
  EXPECT_TRUE(Encryptor::EncryptString16(plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &utf8_result));
  EXPECT_EQ(utf8_plaintext, UTF16ToUTF8(result));

  EXPECT_TRUE(Encryptor::EncryptString(utf8_plaintext, &ciphertext));
  EXPECT_TRUE(Encryptor::DecryptString16(ciphertext, &result));
  EXPECT_EQ(plaintext, result);
  EXPECT_TRUE(Encryptor::DecryptString(ciphertext, &utf8_result));
  EXPECT_EQ(utf8_plaintext, UTF16ToUTF8(result));
}

TEST_F(EncryptorTest, EncryptionDecryption) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // Test borderline cases (empty strings).
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Test a simple string.
  plaintext = "hello";
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, result);

  // Make sure it null terminates.
  plaintext.assign("hello", 3);
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_EQ(plaintext, "hel");
}

TEST_F(EncryptorTest, CypherTextDiffers) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // Test borderline cases (empty strings).
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  // |cyphertext| is empty on the Mac, different on Windows.
  EXPECT_TRUE(ciphertext.empty() || plaintext != ciphertext);
  EXPECT_EQ(plaintext, result);

  // Test a simple string.
  plaintext = "hello";
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_NE(plaintext, ciphertext);
  EXPECT_EQ(plaintext, result);

  // Make sure it null terminates.
  plaintext.assign("hello", 3);
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  ASSERT_TRUE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_NE(plaintext, ciphertext);
  EXPECT_EQ(result, "hel");
}

TEST_F(EncryptorTest, DecryptError) {
  std::string plaintext;
  std::string result;
  std::string ciphertext;

  // Test a simple string, messing with ciphertext prior to decrypting.
  plaintext = "hello";
  ASSERT_TRUE(Encryptor::EncryptString(plaintext, &ciphertext));
  EXPECT_NE(plaintext, ciphertext);
  ASSERT_LT(4UL, ciphertext.size());
  ciphertext[3] = ciphertext[3] + 1;
  EXPECT_FALSE(Encryptor::DecryptString(ciphertext, &result));
  EXPECT_NE(plaintext, result);
  EXPECT_TRUE(result.empty());
}

}  // namespace
