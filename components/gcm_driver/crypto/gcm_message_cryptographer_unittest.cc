// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// The number of bits of the key in AEAD_AES_128_GCM.
const size_t kKeySizeBits = 128;

// Example plaintext data to use in the tests.
const char kExamplePlaintext[] = "Example plaintext";

// A test vector contains the information necessary to either encrypt or decrypt
// a message. These vectors were created using a JavaScript implementation of
// the same RFCs that the GCMMessageCryptographer implements.
struct TestVector {
  const char* const input;
  const char* const key;
  const char* const salt;
  size_t record_size;
  const char* const output;
};

const TestVector kEncryptionTestVectors[] = {
  // Simple message.
  { "Hello, world!",
    "AhA6n2oFYPWIh+cXwyv1m2C0JvmjHB4ZkXj8QylESXU=",
    "tsJYqAGvFDk6lDEv7daecw==",
    4096,
    "KNXRqBR9VKhtajeMaeKR/rHYIiORcyeFpUwWFGyS"
   },
   // Empty message.
   { "",
    "lMyvTong4VR053jfCpWmMDGW5dEDAqiTZUIU+inhTjU=",
    "wH3uvZqcN6oey9whiGpn1A==",
    4096,
    "Mnfr+AU5o7D30gjFdVOTFtw="
   },
   // Message with an invalid salt size.
   { "Hello, world!",
    "CcdxzkR6z1EY9vSrM7/IxYVxDxu46hV638EZQTPd7XI=",
    "aRr1fI1YSGVi5XU=",
    4096,
    nullptr  // expected to fail
   }
};

const TestVector kDecryptionTestVectors[] = {
  // Simple message.
  { "avAFNhdbQohzizu+ORbU4XHhHSaXzw9lTN7UzB/j",
    "47ZytAw9qHlm+Q8g+7rH81rUPzaCgGcoFvlS1qxQtQk=",
    "EuR7EVetcaWpndXd/dKeyA==",
    4096,
    "Hello, world!"
   },
   // Simple message with 16 bytes of padding.
   { "0198n7ZJ/ZPMnl4ZU2l9Lma5ktKbuzXCiJEXyYtROmWTP8RSiZd8sUd48xpk6Q==",
     "MYSsNybwrTzRIzQYUq/yFPc6ugcTrJdEZJDM4NswvUg=",
     "8sEAMQYnufo2UkKl80cUGQ==",
     4096,
     "Hello, world!"
   },
   // Empty message.
   { "g+ACk32a4gK2dS2xllKXn4c=",
     "S3+Ki/+XtzR66gUp/zR75CC5JXO62pyr5fWfneTYwFE=",
     "4RM6s19jJHdmqiVEJDp9jg==",
     4096,
     ""
   },
   // Message with an invalid salt size.
   { "rt4OiodS087DAQo6e24wA55k0hRPAHgz7OX7m+nj",
     "wW3Iy5ma803lLd+ysPdHUe2NB3HqXbY0XhCCdG5Y1Gw=",
     "N7oMH/xohAhMhOY=",
     4096,
     nullptr  // expected to fail
   },
   // Message with an invalid record size.
   { "AsuoRkFtqLE1c0mGCae4OvgZSCSHWCoeRL9mXKjY",
     "omxWz7tse3lgDpxUP+e7u14Dp1irvV3BdzXTcZOtsHs=",
     "vKJD3bexto1hY64KVzS7ug==",
     3,
     nullptr  // expected to fail
   },
   // Truncated message.
   { "AGr4BfZSXW9txWkAG8pjg7IuRWWm1Mo8bDli/PSv",
    "kR5BMfqMKOD1yrLKE2giObXHI7merrMtnoO2oqneqXA=",
    "SQeJSPrqHvTdSfAMF8bBzQ==",
    13,
    nullptr  // expected to fail
   },
   // Message with multiple (2) records.
   { "H2ujfPbpRbVSy+adIG2NRe4VxkX4V0r/zl6e9xnMSF6LSutblGdWLrwQc82Xh7DXAQlihW0q3"
         "IQzHP+LIxuAiA==",
     "W3W4gx7sqcfmBnvNNdO9d4MBCC1bvJkvsNjZOGD+CCg=",
     "xG0TPGi9aIcxjpXKmaYBBQ==",
     7,
     nullptr  // expected to fail
   }
};

}  // namespace

class GCMMessageCryptographerTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_ptr<crypto::SymmetricKey> random_key(
        crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES,
                                                kKeySizeBits));

    ASSERT_TRUE(random_key->GetRawKey(&key_));
  }

 protected:
  // Generates a cryptographically secure random salt of 16-octets in size, the
  // required length as expected by the HKDF.
  std::string GenerateRandomSalt() {
    const size_t kSaltSize = 16;

    std::string salt;

    crypto::RandBytes(base::WriteInto(&salt, kSaltSize + 1), kSaltSize);
    return salt;
  }

  GCMMessageCryptographer* cryptographer() { return &cryptographer_; }

  base::StringPiece key() const { return key_; }

 private:
  GCMMessageCryptographer cryptographer_;

  std::string key_;
};

TEST_F(GCMMessageCryptographerTest, RoundTrip) {
  const std::string salt = GenerateRandomSalt();

  size_t record_size = 0;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer()->Encrypt(kExamplePlaintext, key(), salt,
                                       &record_size, &ciphertext));

  EXPECT_GT(record_size, ciphertext.size() - 16);
  EXPECT_GT(ciphertext.size(), 0u);

  ASSERT_TRUE(cryptographer()->Decrypt(ciphertext, key(), salt, record_size,
                                       &plaintext));

  EXPECT_EQ(kExamplePlaintext, plaintext);
}

TEST_F(GCMMessageCryptographerTest, RoundTripEmptyMessage) {
  const std::string salt = GenerateRandomSalt();
  const std::string message = "";

  size_t record_size = 0;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer()->Encrypt(message, key(), salt, &record_size,
                                       &ciphertext));

  EXPECT_GT(record_size, ciphertext.size() - 16);
  EXPECT_GT(ciphertext.size(), 0u);

  ASSERT_TRUE(cryptographer()->Decrypt(ciphertext, key(), salt, record_size,
                                       &plaintext));

  EXPECT_EQ(message, plaintext);
}

TEST_F(GCMMessageCryptographerTest, InvalidRecordSize) {
  const std::string salt = GenerateRandomSalt();

  size_t record_size = 0;

  std::string ciphertext, plaintext;
  EXPECT_TRUE(cryptographer()->Encrypt(kExamplePlaintext, key(), salt,
                                       &record_size, &ciphertext));

  EXPECT_GT(record_size, ciphertext.size() - 16);
  EXPECT_FALSE(cryptographer()->Decrypt(ciphertext, key(), salt,
                                        0 /* record_size */, &plaintext));

  EXPECT_FALSE(cryptographer()->Decrypt(ciphertext, key(), salt,
                                        ciphertext.size() - 17, &plaintext));

  EXPECT_TRUE(cryptographer()->Decrypt(ciphertext, key(), salt,
                                       ciphertext.size() - 16, &plaintext));
}

TEST_F(GCMMessageCryptographerTest, InvalidRecordPadding) {
  std::string message = std::string(1, '\0') + kExamplePlaintext;

  const std::string salt = GenerateRandomSalt();

  const std::string nonce = cryptographer()->DeriveNonce(key(), salt);
  const std::string content_encryption_key =
      cryptographer()->DeriveContentEncryptionKey(key(), salt);

  ASSERT_GT(message.size(), 1u);
  const size_t record_size = message.size() + 1;

  std::string ciphertext, plaintext;
  ASSERT_TRUE(cryptographer()->EncryptDecryptRecordInternal(
      GCMMessageCryptographer::ENCRYPT, message, content_encryption_key, nonce,
      &ciphertext));

  ASSERT_TRUE(cryptographer()->Decrypt(ciphertext, key(), salt, record_size,
                                       &plaintext));

  // Note that GCMMessageCryptographer::Decrypt removes the padding.
  EXPECT_EQ(kExamplePlaintext, plaintext);

  // Now run the same steps again, but say that there are four padding octets.
  // This should be rejected because the padding will not be all zeros.
  message[0] = 4;

  ASSERT_TRUE(cryptographer()->EncryptDecryptRecordInternal(
      GCMMessageCryptographer::ENCRYPT, message, content_encryption_key, nonce,
      &ciphertext));

  ASSERT_FALSE(cryptographer()->Decrypt(ciphertext, key(), salt, record_size,
                                        &plaintext));

  // Run the same steps again, but say that there are more padding octets than
  // the length of the message.
  message[0] = 64;

  EXPECT_GT(static_cast<size_t>(message[0]), message.size());
  ASSERT_TRUE(cryptographer()->EncryptDecryptRecordInternal(
      GCMMessageCryptographer::ENCRYPT, message, content_encryption_key, nonce,
      &ciphertext));

  ASSERT_FALSE(cryptographer()->Decrypt(ciphertext, key(), salt, record_size,
                                        &plaintext));
}

TEST_F(GCMMessageCryptographerTest, EncryptionTestVectors) {
  std::string key, salt, output, ciphertext;
  size_t record_size = 0;

  for (size_t i = 0; i < arraysize(kEncryptionTestVectors); ++i) {
    SCOPED_TRACE(i);

    ASSERT_TRUE(base::Base64Decode(kEncryptionTestVectors[i].key, &key));
    ASSERT_TRUE(base::Base64Decode(kEncryptionTestVectors[i].salt, &salt));

    const bool has_output = kEncryptionTestVectors[i].output;
    const bool result = cryptographer()->Encrypt(
        kEncryptionTestVectors[i].input, key, salt, &record_size, &ciphertext);

    if (!has_output) {
      EXPECT_FALSE(result);
      continue;
    }

    EXPECT_TRUE(result);
    ASSERT_TRUE(base::Base64Decode(kEncryptionTestVectors[i].output,
                &output));

    EXPECT_EQ(kEncryptionTestVectors[i].record_size, record_size);
    EXPECT_EQ(output, ciphertext);
  }
}

TEST_F(GCMMessageCryptographerTest, DecryptionTestVectors) {
  std::string input, key, salt, plaintext;
  for (size_t i = 0; i < arraysize(kDecryptionTestVectors); ++i) {
    SCOPED_TRACE(i);

    ASSERT_TRUE(base::Base64Decode(kDecryptionTestVectors[i].input, &input));
    ASSERT_TRUE(base::Base64Decode(kDecryptionTestVectors[i].key, &key));
    ASSERT_TRUE(base::Base64Decode(kDecryptionTestVectors[i].salt, &salt));

    const bool has_output = kDecryptionTestVectors[i].output;
    const bool result = cryptographer()->Decrypt(
        input, key, salt, kDecryptionTestVectors[i].record_size, &plaintext);

    if (!has_output) {
      EXPECT_FALSE(result);
      continue;
    }

    EXPECT_TRUE(result);
    EXPECT_EQ(kDecryptionTestVectors[i].output, plaintext);
  }
}

}  // namespace gcm
