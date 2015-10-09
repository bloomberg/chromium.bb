// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_encryption_provider.h"

#include <sstream>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "components/gcm_driver/common/gcm_messages.h"
#include "components/gcm_driver/crypto/gcm_key_store.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "crypto/curve25519.h"
#include "crypto/random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

const char kExampleAppId[] = "my-app-id";
const char kExampleMessage[] = "Hello, world, this is the GCM Driver!";

const char kValidEncryptionHeader[] =
    "keyid=foo;salt=MTIzNDU2Nzg5MDEyMzQ1Ng;rs=1024";
const char kInvalidEncryptionHeader[] = "keyid";

const char kValidEncryptionKeyHeader[] =
    "keyid=foo;dh=NjU0MzIxMDk4NzY1NDMyMTEyMzQ1Njc4OTAxMjM0NTY";
const char kInvalidEncryptionKeyHeader[] = "keyid";

// TODO(peter): Unify the Base64Url implementations. https://crbug.com/536745.
void Base64UrlEncode(const std::string& decoded_input,
                     std::string* encoded_output) {
  base::Base64Encode(decoded_input, encoded_output);
  base::ReplaceChars(*encoded_output, "+", "-", encoded_output);
  base::ReplaceChars(*encoded_output, "/", "_", encoded_output);
}

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

  // To be used as a callback for GCMEncryptionProvider::GetPublicKey().
  void DidGetPublicKey(std::string* key_out, const std::string& key) {
    *key_out = key;
  }

  // To be used as a callback for GCMKeyStore::CreateKeys().
  void DidCreateKeys(KeyPair* pair_out, const KeyPair& pair) {
    *pair_out = pair;
  }

 protected:
  // Tri-state enumaration listing whether the decryption operation is idle
  // (hasn't started yet), succeeded or failed.
  enum DecryptionResult {
    DECRYPTION_IDLE,
    DECRYPTION_SUCCEEDED,
    DECRYPTION_FAILED
  };

  // Decrypts the |message| and then synchronously waits until either the
  // success or failure callbacks has been invoked.
  void Decrypt(const IncomingMessage& message) {
    decryption_result_ = DECRYPTION_IDLE;
    encryption_provider_->DecryptMessage(
        kExampleAppId, message,
        base::Bind(&GCMEncryptionProviderTest::OnDecryptionSucceeded,
                   base::Unretained(this)),
        base::Bind(&GCMEncryptionProviderTest::OnDecryptionFailed,
                   base::Unretained(this)));

    // The encryption keys will be read asynchronously.
    base::RunLoop().RunUntilIdle();

    ASSERT_NE(decryption_result_, DECRYPTION_IDLE);
  }

  DecryptionResult decryption_result() { return decryption_result_; }

  const IncomingMessage& decrypted_message() { return decrypted_message_; }

  GCMEncryptionProvider::DecryptionFailure failure_reason() {
    return failure_reason_;
  }

  GCMEncryptionProvider* encryption_provider() {
    return encryption_provider_.get();
  }

 private:
  void OnDecryptionSucceeded(const IncomingMessage& message) {
    decryption_result_ = DECRYPTION_SUCCEEDED;
    decrypted_message_ = message;
  }

  void OnDecryptionFailed(GCMEncryptionProvider::DecryptionFailure reason) {
    decryption_result_ = DECRYPTION_FAILED;
    failure_reason_ = reason;
  }

  base::MessageLoop message_loop_;
  base::ScopedTempDir scoped_temp_dir_;

  scoped_ptr<GCMEncryptionProvider> encryption_provider_;

  DecryptionResult decryption_result_ = DECRYPTION_IDLE;
  GCMEncryptionProvider::DecryptionFailure failure_reason_ =
      GCMEncryptionProvider::DECRYPTION_FAILURE_UNKNOWN;

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
  double_header_message.data["encryption_key"] = "";
  EXPECT_FALSE(encryption_provider()->IsEncryptedMessage(
                   double_header_message));

  IncomingMessage double_header_with_data_message;
  double_header_with_data_message.data["encryption"] = "";
  double_header_with_data_message.data["encryption_key"] = "";
  double_header_with_data_message.raw_data = "foo";
  EXPECT_TRUE(encryption_provider()->IsEncryptedMessage(
                  double_header_with_data_message));
}

TEST_F(GCMEncryptionProviderTest, VerifiesEncryptionHeaderParsing) {
  // The Encryption header must be parsable and contain valid values.
  // Note that this is more extensively tested in EncryptionHeaderParsersTest.

  IncomingMessage invalid_message;
  invalid_message.data["encryption"] = kInvalidEncryptionHeader;
  invalid_message.data["encryption_key"] = kValidEncryptionKeyHeader;

  ASSERT_NO_FATAL_FAILURE(Decrypt(invalid_message));
  ASSERT_EQ(DECRYPTION_FAILED, decryption_result());
  EXPECT_EQ(GCMEncryptionProvider::DECRYPTION_FAILURE_INVALID_ENCRYPTION_HEADER,
            failure_reason());

  IncomingMessage valid_message;
  valid_message.data["encryption"] = kValidEncryptionHeader;
  valid_message.data["encryption_key"] = kInvalidEncryptionKeyHeader;

  ASSERT_NO_FATAL_FAILURE(Decrypt(valid_message));
  ASSERT_EQ(DECRYPTION_FAILED, decryption_result());
  EXPECT_NE(GCMEncryptionProvider::DECRYPTION_FAILURE_INVALID_ENCRYPTION_HEADER,
            failure_reason());
}

TEST_F(GCMEncryptionProviderTest, VerifiesEncryptionKeyHeaderParsing) {
  // The Encryption-Key header must be parsable and contain valid values.
  // Note that this is more extensively tested in EncryptionHeaderParsersTest.

  IncomingMessage invalid_message;
  invalid_message.data["encryption"] = kValidEncryptionHeader;
  invalid_message.data["encryption_key"] = kInvalidEncryptionKeyHeader;

  ASSERT_NO_FATAL_FAILURE(Decrypt(invalid_message));
  ASSERT_EQ(DECRYPTION_FAILED, decryption_result());
  EXPECT_EQ(
      GCMEncryptionProvider::DECRYPTION_FAILURE_INVALID_ENCRYPTION_KEY_HEADER,
      failure_reason());

  IncomingMessage valid_message;
  valid_message.data["encryption"] = kInvalidEncryptionHeader;
  valid_message.data["encryption_key"] = kValidEncryptionKeyHeader;

  ASSERT_NO_FATAL_FAILURE(Decrypt(valid_message));
  ASSERT_EQ(DECRYPTION_FAILED, decryption_result());
  EXPECT_NE(
      GCMEncryptionProvider::DECRYPTION_FAILURE_INVALID_ENCRYPTION_KEY_HEADER,
      failure_reason());
}

TEST_F(GCMEncryptionProviderTest, VerifiesExistingKeys) {
  // When both headers are valid, the encryption keys still must be known to
  // the GCM key store before the message can be decrypted.

  IncomingMessage message;
  message.data["encryption"] = kValidEncryptionHeader;
  message.data["encryption_key"] = kValidEncryptionKeyHeader;

  ASSERT_NO_FATAL_FAILURE(Decrypt(message));
  ASSERT_EQ(DECRYPTION_FAILED, decryption_result());
  EXPECT_EQ(GCMEncryptionProvider::DECRYPTION_FAILURE_NO_KEYS,
            failure_reason());

  std::string public_key;
  encryption_provider()->GetPublicKey(
      kExampleAppId,
      base::Bind(&GCMEncryptionProviderTest::DidGetPublicKey,
                 base::Unretained(this), &public_key));

  // Getting (or creating) the public key will be done asynchronously.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(crypto::curve25519::kBytes, public_key.size());

  ASSERT_NO_FATAL_FAILURE(Decrypt(message));
  ASSERT_EQ(DECRYPTION_FAILED, decryption_result());
  EXPECT_NE(GCMEncryptionProvider::DECRYPTION_FAILURE_NO_KEYS,
            failure_reason());
}

TEST_F(GCMEncryptionProviderTest, EncryptionRoundTrip) {
  // Performs a full round-trip of the encryption feature, including getting a
  // public/private key-pair and performing the cryptographic operations. This
  // is more of an integration test than a unit test.

  KeyPair pair;
  KeyPair server_pair;

  // Retrieve the public/private key-pair immediately from the key store, given
  // that the GCMEncryptionProvider will only share the public key with users.
  // Also create a second pair, which will act as the server's keys.
  encryption_provider()->key_store_->CreateKeys(
      kExampleAppId,
      base::Bind(&GCMEncryptionProviderTest::DidCreateKeys,
                 base::Unretained(this), &pair));

  encryption_provider()->key_store_->CreateKeys(
      std::string(kExampleAppId) + "-server",
      base::Bind(&GCMEncryptionProviderTest::DidCreateKeys,
                 base::Unretained(this), &server_pair));

  // Creating the public keys will be done asynchronously.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(crypto::curve25519::kScalarBytes, pair.private_key().size());
  ASSERT_EQ(crypto::curve25519::kBytes, pair.public_key().size());

  ASSERT_EQ(crypto::curve25519::kScalarBytes, server_pair.private_key().size());
  ASSERT_EQ(crypto::curve25519::kBytes, server_pair.public_key().size());

  std::string salt;

  // Creates a cryptographically secure salt of |salt_size| octets in size, and
  // calculate the shared secret for the message.
  crypto::RandBytes(base::WriteInto(&salt, 16 + 1), 16);

  uint8_t shared_key[crypto::curve25519::kBytes];
  crypto::curve25519::ScalarMult(
      reinterpret_cast<const unsigned char*>(server_pair.private_key().data()),
      reinterpret_cast<const unsigned char*>(pair.public_key().data()),
      shared_key);

  base::StringPiece shared_key_string_piece(
      reinterpret_cast<char*>(shared_key), crypto::curve25519::kBytes);

  IncomingMessage message;
  size_t record_size;

  // Encrypts the |kExampleMessage| using the generated shared key and the
  // random |salt|, storing the result in |record_size| and the message.
  GCMMessageCryptographer cryptographer;
  ASSERT_TRUE(cryptographer.Encrypt(kExampleMessage, shared_key_string_piece,
                                    salt, &record_size, &message.raw_data));

  std::string encoded_salt, encoded_key;

  // Compile the incoming GCM message, including the required headers.
  Base64UrlEncode(salt, &encoded_salt);
  Base64UrlEncode(server_pair.public_key(), &encoded_key);

  std::stringstream encryption_header;
  encryption_header << "rs=" << base::SizeTToString(record_size) << ";";
  encryption_header << "salt=" << encoded_salt;

  message.data["encryption"] = encryption_header.str();
  message.data["encryption_key"] = "dh=" + encoded_key;

  ASSERT_TRUE(encryption_provider()->IsEncryptedMessage(message));

  // Decrypt the message, and expect everything to go wonderfully well.
  ASSERT_NO_FATAL_FAILURE(Decrypt(message));
  ASSERT_EQ(DECRYPTION_SUCCEEDED, decryption_result());

  EXPECT_TRUE(decrypted_message().decrypted);
  EXPECT_EQ(kExampleMessage, decrypted_message().raw_data);
}

}  // namespace gcm
