// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_encryption_provider.h"

#include <stddef.h>

#include <sstream>
#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "components/gcm_driver/common/gcm_messages.h"
#include "components/gcm_driver/crypto/gcm_key_store.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

const char kExampleAppId[] = "my-app-id";
const char kExampleMessage[] = "Hello, world, this is the GCM Driver!";

const char kValidEncryptionHeader[] =
    "keyid=foo;salt=MTIzNDU2Nzg5MDEyMzQ1Ng;rs=1024";
const char kInvalidEncryptionHeader[] = "keyid";

const char kValidCryptoKeyHeader[] =
    "keyid=foo;dh=BL_UGhfudEkXMUd4U4-D4nP5KHxKjQHsW6j88ybbehXM7fqi1OMFefDUEi0eJ"
    "vsKfyVBWYkQjH-lSPJKxjAyslg";
const char kInvalidCryptoKeyHeader[] = "keyid";

}  // namespace

class GCMEncryptionProviderTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    encryption_provider_.reset(new GCMEncryptionProvider);
    encryption_provider_->Init(scoped_temp_dir_.path(),
                               message_loop_.task_runner());
  }

  void TearDown() override {
    encryption_provider_.reset();

    // |encryption_provider_| owns a ProtoDatabaseImpl whose destructor deletes
    // the underlying LevelDB database on the task runner.
    base::RunLoop().RunUntilIdle();
  }

  // To be used as a callback for GCMEncryptionProvider::GetEncryptionInfo().
  void DidGetEncryptionInfo(std::string* p256dh_out,
                            std::string* auth_secret_out,
                            const std::string& p256dh,
                            const std::string& auth_secret) {
    *p256dh_out = p256dh;
    *auth_secret_out = auth_secret;
  }

  // To be used as a callback for GCMKeyStore::CreateKeys().
  void DidCreateKeys(KeyPair* pair_out, std::string* auth_secret_out,
                     const KeyPair& pair, const std::string& auth_secret) {
    *pair_out = pair;
    *auth_secret_out = auth_secret;
  }

 protected:
  // Decrypts the |message| and then synchronously waits until either the
  // success or failure callbacks has been invoked.
  void Decrypt(const IncomingMessage& message) {
    encryption_provider_->DecryptMessage(
        kExampleAppId, message,
        base::Bind(&GCMEncryptionProviderTest::DidDecryptMessage,
                   base::Unretained(this)));

    // The encryption keys will be read asynchronously.
    base::RunLoop().RunUntilIdle();
  }

  // Returns the result of the previous decryption operation.
  GCMEncryptionProvider::DecryptionResult decryption_result() {
    return decryption_result_;
  }

  // Returns the message resulting from the previous decryption operation.
  const IncomingMessage& decrypted_message() { return decrypted_message_; }

  GCMEncryptionProvider* encryption_provider() {
    return encryption_provider_.get();
  }

 private:
  void DidDecryptMessage(GCMEncryptionProvider::DecryptionResult result,
                         const IncomingMessage& message) {
    decryption_result_ = result;
    decrypted_message_ = message;
  }

  base::MessageLoop message_loop_;
  base::ScopedTempDir scoped_temp_dir_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<GCMEncryptionProvider> encryption_provider_;

  GCMEncryptionProvider::DecryptionResult decryption_result_ =
      GCMEncryptionProvider::DECRYPTION_RESULT_UNENCRYPTED;

  IncomingMessage decrypted_message_;
};

TEST_F(GCMEncryptionProviderTest, IsEncryptedMessage) {
  // Both the Encryption and Encryption-Key headers must be present, and the raw
  // data must be non-empty for a message to be considered encrypted.

  IncomingMessage empty_message;
  EXPECT_FALSE(encryption_provider()->IsEncryptedMessage(empty_message));

  IncomingMessage single_header_message;
  single_header_message.data["encryption"] = "";
  EXPECT_FALSE(encryption_provider()->IsEncryptedMessage(
                   single_header_message));

  IncomingMessage double_header_message;
  double_header_message.data["encryption"] = "";
  double_header_message.data["crypto-key"] = "";
  EXPECT_FALSE(encryption_provider()->IsEncryptedMessage(
                   double_header_message));

  IncomingMessage double_header_with_data_message;
  double_header_with_data_message.data["encryption"] = "";
  double_header_with_data_message.data["crypto-key"] = "";
  double_header_with_data_message.raw_data = "foo";
  EXPECT_TRUE(encryption_provider()->IsEncryptedMessage(
                  double_header_with_data_message));
}

TEST_F(GCMEncryptionProviderTest, VerifiesEncryptionHeaderParsing) {
  // The Encryption header must be parsable and contain valid values.
  // Note that this is more extensively tested in EncryptionHeaderParsersTest.

  IncomingMessage invalid_message;
  invalid_message.data["encryption"] = kInvalidEncryptionHeader;
  invalid_message.data["crypto-key"] = kValidCryptoKeyHeader;
  invalid_message.raw_data = "foo";

  ASSERT_NO_FATAL_FAILURE(Decrypt(invalid_message));
  EXPECT_EQ(GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER,
            decryption_result());

  IncomingMessage valid_message;
  valid_message.data["encryption"] = kValidEncryptionHeader;
  valid_message.data["crypto-key"] = kInvalidCryptoKeyHeader;
  valid_message.raw_data = "foo";

  ASSERT_NO_FATAL_FAILURE(Decrypt(valid_message));
  EXPECT_NE(GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_ENCRYPTION_HEADER,
            decryption_result());
}

TEST_F(GCMEncryptionProviderTest, VerifiesCryptoKeyHeaderParsing) {
  // The Encryption-Key header must be parsable and contain valid values.
  // Note that this is more extensively tested in EncryptionHeaderParsersTest.

  IncomingMessage invalid_message;
  invalid_message.data["encryption"] = kValidEncryptionHeader;
  invalid_message.data["crypto-key"] = kInvalidCryptoKeyHeader;
  invalid_message.raw_data = "foo";

  ASSERT_NO_FATAL_FAILURE(Decrypt(invalid_message));
  EXPECT_EQ(GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER,
            decryption_result());

  IncomingMessage valid_message;
  valid_message.data["encryption"] = kInvalidEncryptionHeader;
  valid_message.data["crypto-key"] = kValidCryptoKeyHeader;
  valid_message.raw_data = "foo";

  ASSERT_NO_FATAL_FAILURE(Decrypt(valid_message));
  EXPECT_NE(GCMEncryptionProvider::DECRYPTION_RESULT_INVALID_CRYPTO_KEY_HEADER,
            decryption_result());
}

TEST_F(GCMEncryptionProviderTest, VerifiesExistingKeys) {
  // When both headers are valid, the encryption keys still must be known to
  // the GCM key store before the message can be decrypted.

  IncomingMessage message;
  message.data["encryption"] = kValidEncryptionHeader;
  message.data["crypto-key"] = kValidCryptoKeyHeader;
  message.raw_data = "foo";

  ASSERT_NO_FATAL_FAILURE(Decrypt(message));
  EXPECT_EQ(GCMEncryptionProvider::DECRYPTION_RESULT_NO_KEYS,
            decryption_result());

  std::string public_key, auth_secret;
  encryption_provider()->GetEncryptionInfo(
      kExampleAppId,
      base::Bind(&GCMEncryptionProviderTest::DidGetEncryptionInfo,
                 base::Unretained(this), &public_key, &auth_secret));

  // Getting (or creating) the public key will be done asynchronously.
  base::RunLoop().RunUntilIdle();

  ASSERT_GT(public_key.size(), 0u);
  ASSERT_GT(auth_secret.size(), 0u);

  ASSERT_NO_FATAL_FAILURE(Decrypt(message));
  EXPECT_NE(GCMEncryptionProvider::DECRYPTION_RESULT_NO_KEYS,
            decryption_result());
}

TEST_F(GCMEncryptionProviderTest, EncryptionRoundTrip) {
  // Performs a full round-trip of the encryption feature, including getting a
  // public/private key-pair and performing the cryptographic operations. This
  // is more of an integration test than a unit test.

  KeyPair pair, server_pair;
  std::string auth_secret, server_authentication;

  // Retrieve the public/private key-pair immediately from the key store, given
  // that the GCMEncryptionProvider will only share the public key with users.
  // Also create a second pair, which will act as the server's keys.
  encryption_provider()->key_store_->CreateKeys(
      kExampleAppId,
      base::Bind(&GCMEncryptionProviderTest::DidCreateKeys,
                 base::Unretained(this), &pair, &auth_secret));

  encryption_provider()->key_store_->CreateKeys(
      std::string(kExampleAppId) + "-server",
      base::Bind(&GCMEncryptionProviderTest::DidCreateKeys,
                 base::Unretained(this), &server_pair, &server_authentication));

  // Creating the public keys will be done asynchronously.
  base::RunLoop().RunUntilIdle();

  ASSERT_GT(pair.public_key().size(), 0u);
  ASSERT_GT(server_pair.public_key().size(), 0u);

  ASSERT_GT(pair.private_key().size(), 0u);
  ASSERT_GT(server_pair.private_key().size(), 0u);

  std::string salt;

  // Creates a cryptographically secure salt of |salt_size| octets in size, and
  // calculate the shared secret for the message.
  crypto::RandBytes(base::WriteInto(&salt, 16 + 1), 16);

  std::string shared_secret;
  ASSERT_TRUE(ComputeSharedP256Secret(
      pair.private_key(), pair.public_key_x509(), server_pair.public_key(),
      &shared_secret));

  IncomingMessage message;
  size_t record_size;

  // Encrypts the |kExampleMessage| using the generated shared key and the
  // random |salt|, storing the result in |record_size| and the message.
  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Label::P256, pair.public_key(),
      server_pair.public_key(), auth_secret);

  ASSERT_TRUE(cryptographer.Encrypt(kExampleMessage, shared_secret, salt,
                                    &record_size, &message.raw_data));

  std::string encoded_salt, encoded_key;

  // Compile the incoming GCM message, including the required headers.
  base::Base64UrlEncode(
      salt, base::Base64UrlEncodePolicy::INCLUDE_PADDING, &encoded_salt);
  base::Base64UrlEncode(
      server_pair.public_key(), base::Base64UrlEncodePolicy::INCLUDE_PADDING,
      &encoded_key);

  std::stringstream encryption_header;
  encryption_header << "rs=" << base::SizeTToString(record_size) << ";";
  encryption_header << "salt=" << encoded_salt;

  message.data["encryption"] = encryption_header.str();
  message.data["crypto-key"] = "dh=" + encoded_key;

  ASSERT_TRUE(encryption_provider()->IsEncryptedMessage(message));

  // Decrypt the message, and expect everything to go wonderfully well.
  ASSERT_NO_FATAL_FAILURE(Decrypt(message));
  ASSERT_EQ(GCMEncryptionProvider::DECRYPTION_RESULT_DECRYPTED,
            decryption_result());

  EXPECT_TRUE(decrypted_message().decrypted);
  EXPECT_EQ(kExampleMessage, decrypted_message().raw_data);
}

}  // namespace gcm
