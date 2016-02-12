// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <stddef.h>

#include "base/base64url.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// The number of bits of the key in AEAD_AES_128_GCM.
const size_t kKeySizeBits = 128;

// Example plaintext data to use in the tests.
const char kExamplePlaintext[] = "Example plaintext";

// Fixed local and peer public keys must be used to get consistent results.
const char kLocalPublicKeyCommon[] =
    "BIXzEKOFquzVlr_1tS1bhmobZU3IJq2bswDflMJsizixqd_HFSvCJaCAotNjBw6A-iKQk7FshA"
    "jdAA-T9Rh1a7U";

const char kPeerPublicKeyCommon[] =
    "BAuzSrdIyKZsHnuOhqklkIKi6fl65V9OdPy6nFwI2SywL5-6I5SkkDtfIL9y7NkoEE345jv2Eo"
    "5n4NIbLJIBjTM";

const char kAuthSecretCommon[] = "MyAuthenticationSecret";

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
    "AhA6n2oFYPWIh-cXwyv1m2C0JvmjHB4ZkXj8QylESXU",
    "tsJYqAGvFDk6lDEv7daecw",
    4096,
    "FgWrrnZq79oI_N4ORkVLHx1jfVmjeiIk-xFX8PzVuA"
   },
   // Empty message.
   { "",
    "lMyvTong4VR053jfCpWmMDGW5dEDAqiTZUIU-inhTjU",
    "wH3uvZqcN6oey9whiGpn1A",
    4096,
    "MTI9zZ8CJTUzbZ4qNDoQZs0k"
   },
   // Message with an invalid salt size.
   { "Hello, world!",
    "CcdxzkR6z1EY9vSrM7_IxYVxDxu46hV638EZQTPd7XI",
    "aRr1fI1YSGVi5XU",
    4096,
    nullptr  // expected to fail
   }
};

const TestVector kDecryptionTestVectors[] = {
  // Simple message.
  { "ceiEu_YpmqLoakD4smdzvy2XKRQrJ9vBzB2aqYEfzw",
    "47ZytAw9qHlm-Q8g-7rH81rUPzaCgGcoFvlS1qxQtQk",
    "EuR7EVetcaWpndXd_dKeyA",
    4096,
    "Hello, world!"
   },
   // Simple message with 16 bytes of padding.
   { "WSf6fz1O0aIJyaPTCnvk83OqIQxsRKeFOvblPLsPpFB_1AV9ROk09TE1cGrB6zQ",
     "MYSsNybwrTzRIzQYUq_yFPc6ugcTrJdEZJDM4NswvUg",
     "8sEAMQYnufo2UkKl80cUGQ",
     4096,
     "Hello, world!"
   },
   // Empty message.
   { "Ur3vHedGDO5IPYDvbhHYjbjG",
     "S3-Ki_-XtzR66gUp_zR75CC5JXO62pyr5fWfneTYwFE",
     "4RM6s19jJHdmqiVEJDp9jg",
     4096,
     ""
   },
   // Message with an invalid salt size.
   { "iGrOpmJC5XTTf7wtgdhZ_qT",
     "wW3Iy5ma803lLd-ysPdHUe2NB3HqXbY0XhCCdG5Y1Gw",
     "N7oMH_xohAhMhOY",
     4096,
     nullptr  // expected to fail
   },
   // Message with an invalid record size.
   { "iGrOpmJC5XTTf7wtgdhZ_qT",
     "kR5BMfqMKOD1yrLKE2giObXHI7merrMtnoO2oqneqXA",
     "SQeJSPrqHvTdSfAMF8bBzQ",
     8,
     nullptr  // expected to fail
   },
   // Message with multiple (2) records.
   { "RqQVHRXlfYjzW9xhzh3V_KijLKjZiKzGXosqN_IaMzi0zI0tXXhC1urtrk3iWRoqttNXpkD2r"
         "UCgLy8A1FnTjw",
     "W3W4gx7sqcfmBnvNNdO9d4MBCC1bvJkvsNjZOGD-CCg",
     "xG0TPGi9aIcxjpXKmaYBBQ",
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

    std::string local_public_key, peer_public_key;
    ASSERT_TRUE(base::Base64UrlDecode(
        kLocalPublicKeyCommon, base::Base64UrlDecodePolicy::IGNORE_PADDING,
        &local_public_key));
    ASSERT_TRUE(base::Base64UrlDecode(
        kPeerPublicKeyCommon, base::Base64UrlDecodePolicy::IGNORE_PADDING,
        &peer_public_key));

    cryptographer_.reset(
        new GCMMessageCryptographer(GCMMessageCryptographer::Label::P256,
                                    local_public_key, peer_public_key,
                                    kAuthSecretCommon));
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

  GCMMessageCryptographer* cryptographer() { return cryptographer_.get(); }

  base::StringPiece key() const { return key_; }

 private:
  scoped_ptr<GCMMessageCryptographer> cryptographer_;

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
  ASSERT_TRUE(cryptographer()->Encrypt(kExamplePlaintext, key(), salt,
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
  std::string message = std::string(sizeof(uint16_t), '\0') + kExamplePlaintext;

  const std::string salt = GenerateRandomSalt();

  const std::string prk = cryptographer()->DerivePseudoRandomKey(key());
  const std::string nonce = cryptographer()->DeriveNonce(prk, salt);
  const std::string content_encryption_key =
      cryptographer()->DeriveContentEncryptionKey(prk, salt);

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

  // Do the same but changing the second octet indicating padding size, leaving
  // the first octet at zero.
  message[0] = 0;
  message[1] = 4;

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

    ASSERT_TRUE(base::Base64UrlDecode(
        kEncryptionTestVectors[i].key,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &key));
    ASSERT_TRUE(base::Base64UrlDecode(
        kEncryptionTestVectors[i].salt,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &salt));

    const bool has_output = kEncryptionTestVectors[i].output;
    const bool result = cryptographer()->Encrypt(
        kEncryptionTestVectors[i].input, key, salt, &record_size, &ciphertext);

    if (!has_output) {
      EXPECT_FALSE(result);
      continue;
    }

    EXPECT_TRUE(result);
    ASSERT_TRUE(base::Base64UrlDecode(
        kEncryptionTestVectors[i].output,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &output));

    EXPECT_EQ(kEncryptionTestVectors[i].record_size, record_size);
    EXPECT_EQ(output, ciphertext);
  }
}

TEST_F(GCMMessageCryptographerTest, DecryptionTestVectors) {
  std::string input, key, salt, plaintext;
  for (size_t i = 0; i < arraysize(kDecryptionTestVectors); ++i) {
    SCOPED_TRACE(i);

    ASSERT_TRUE(base::Base64UrlDecode(
        kDecryptionTestVectors[i].input,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &input));
    ASSERT_TRUE(base::Base64UrlDecode(
        kDecryptionTestVectors[i].key,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &key));
    ASSERT_TRUE(base::Base64UrlDecode(
        kDecryptionTestVectors[i].salt,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &salt));

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

TEST_F(GCMMessageCryptographerTest, AuthSecretAffectsIKM) {
  std::string public_key;
  ASSERT_TRUE(base::Base64UrlDecode(
      kLocalPublicKeyCommon, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &public_key));

  // Fake IKM to use in the DerivePseudoRandomKey calls.
  const char kFakeIKM[] = "HelloWorld";

  GCMMessageCryptographer hello_cryptographer(
      GCMMessageCryptographer::Label::P256, public_key, public_key, "Hello");

  GCMMessageCryptographer world_cryptographer(
      GCMMessageCryptographer::Label::P256, public_key, public_key, "World");

  ASSERT_NE(hello_cryptographer.DerivePseudoRandomKey(kFakeIKM), kFakeIKM);
  ASSERT_NE(world_cryptographer.DerivePseudoRandomKey(kFakeIKM), kFakeIKM);

  ASSERT_NE(hello_cryptographer.DerivePseudoRandomKey(kFakeIKM),
            world_cryptographer.DerivePseudoRandomKey(kFakeIKM));

  std::string salt = GenerateRandomSalt();

  // Verify that the IKM actually gets used by the transformations.
  size_t hello_record_size, world_record_size;
  std::string hello_ciphertext, world_ciphertext;

  ASSERT_TRUE(hello_cryptographer.Encrypt(kExamplePlaintext, key(), salt,
                                          &hello_record_size,
                                          &hello_ciphertext));
  ASSERT_TRUE(world_cryptographer.Encrypt(kExamplePlaintext, key(), salt,
                                          &world_record_size,
                                          &world_ciphertext));

  // If the ciphertexts differ despite the same key and salt, it got used.
  ASSERT_NE(hello_ciphertext, world_ciphertext);

  // Verify that the different ciphertexts can also be translated back to the
  // plaintext content. This will fail if the auth secret isn't considered.
  std::string hello_plaintext, world_plaintext;

  ASSERT_TRUE(hello_cryptographer.Decrypt(hello_ciphertext, key(), salt,
                                          hello_record_size, &hello_plaintext));
  ASSERT_TRUE(world_cryptographer.Decrypt(world_ciphertext, key(), salt,
                                          world_record_size, &world_plaintext));

  EXPECT_EQ(kExamplePlaintext, hello_plaintext);
  EXPECT_EQ(kExamplePlaintext, world_plaintext);
}

// Reference test against the HTTP encryption coding IETF draft. Both the
// encrypting and decrypting routines of the GCMMessageCryptographer are
// covered by this test.
//
// https://tools.ietf.org/html/draft-thomson-http-encryption#section-5.5
TEST_F(GCMMessageCryptographerTest, ReferenceTest) {
  // base64url-encoded representation of the 16 octet salt.
  const char kSalt[] = "Qg61ZJRva_XBE9IEUelU3A";

  // base64url-encoded representation of the ciphertext, and the plaintext as
  // a normal character array.
  const char kCiphertext[] = "yqD2bapcx14XxUbtwjiGx69eHE3Yd6AqXcwBpT2Kd1uy";
  const char kPlaintext[] = "I am the walrus";

  // Private keys of the sender and receiver represented as ASN.1-encoded PKCS
  // #8 EncryptedPrivateKeyInfo blocks, as required by the ECPrivateKey.
  const char kReceiverPrivate[] =
      "MIGxMBwGCiqGSIb3DQEMAQMwDgQIqMt4d7uJdt4CAggABIGQeikRHE3CqUeF-uUtJno9BL0g"
      "mNRyDihZe8P3nF_g-NYVzvdQowsXfYeza6OQOdDuMXxnGgNToVy2jsiWVN6rxCaSMTY622y8"
      "ajW5voSdqC2PakQ8ZNTPNHarLDMC9NpgGKrUh8hfRLhvb7vtbKIWmx-22rQB5yTYdqzN2m7A"
      "GHMWRnVk0mMzMsMjZqYFaa2D";

  const char kSenderPrivate[] =
      "MIGxMBwGCiqGSIb3DQEMAQMwDgQIFfJ62c9VwXgCAggABIGQkRxDRPQjwuWp1C3-z1pYTDqF"
      "_NZ1kbPsjmkC3JSv02oAYHtBAtKa2e3oAPqsPfCvoCJBJs6G4WY4EuEO1YFL6RKpNl3DpIUc"
      "v9ShR27p_je_nyLpNBAxn2drnjlF_K6s4gcJmcvCxuNjAwOlLMPvQqGjOR2K_oMs1Hdq0EKJ"
      "NwWt3WUVEpuQF_WhYjCVIeGO";

  // Public keys of the sender and receiver represented as uncompressed points,
  // and X.509 SubjectPublicKeyInfo blocks as required by NSS.
  const char kReceiverPublicUncompressed[] =
      "BCEkBjzL8Z3C-oi2Q7oE5t2Np-p7osjGLg93qUP0wvqRT21EEWyf0cQDQcakQMqz4hQKYOQ3"
      "il2nNZct4HgAUQU";
  const char kReceiverPublicX509[] =
      "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEISQGPMvxncL6iLZDugTm3Y2n6nuiyMYuD3ep"
      "Q_TC-pFPbUQRbJ_RxANBxqRAyrPiFApg5DeKXac1ly3geABRBQ";

  const char kSenderPublicUncompressed[] =
      "BDgpRKok2GZZDmS4r63vbJSUtcQx4Fq1V58-6-3NbZzSTlZsQiCEDTQy3CZ0ZMsqeqsEb7qW"
      "2blQHA4S48fynTk";
  const char kSenderPublicX509[] =
      "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEOClEqiTYZlkOZLivre9slJS1xDHgWrVXnz7r"
      "7c1tnNJOVmxCIIQNNDLcJnRkyyp6qwRvupbZuVAcDhLjx_KdOQ";

  // Convert the salt and the ciphertext to binary representations.
  std::string salt, reference_ciphertext;

  ASSERT_TRUE(base::Base64UrlDecode(
      kSalt, base::Base64UrlDecodePolicy::IGNORE_PADDING, &salt));
  ASSERT_TRUE(base::Base64UrlDecode(
      kCiphertext, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &reference_ciphertext));

  // Convert the public and private keys to binary representations.
  std::string receiver_private, receiver_public, receiver_public_x509;
  std::string sender_private, sender_public, sender_public_x509;

  ASSERT_TRUE(base::Base64UrlDecode(
      kReceiverPrivate, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &receiver_private));
  ASSERT_TRUE(base::Base64UrlDecode(
      kReceiverPublicUncompressed, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &receiver_public));
  ASSERT_TRUE(base::Base64UrlDecode(
      kReceiverPublicX509, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &receiver_public_x509));

  ASSERT_TRUE(base::Base64UrlDecode(
      kSenderPrivate, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &sender_private));
  ASSERT_TRUE(base::Base64UrlDecode(
      kSenderPublicUncompressed, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &sender_public));
  ASSERT_TRUE(base::Base64UrlDecode(
      kSenderPublicX509, base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &sender_public_x509));

  // Compute the shared secret between the sender and the receiver's keys.
  std::string sender_shared_secret, receiver_shared_secret;

  ASSERT_TRUE(ComputeSharedP256Secret(sender_private, sender_public_x509,
                                      receiver_public, &sender_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(receiver_private, receiver_public_x509,
                                      sender_public, &receiver_shared_secret));

  ASSERT_GT(sender_shared_secret.size(), 0u);
  ASSERT_EQ(sender_shared_secret, receiver_shared_secret);

  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Label::P256, receiver_public, sender_public,
      "" /* auth_secret */);

  // The reference vectors do not use an authentication secret.
  cryptographer.set_allow_empty_auth_secret_for_tests(true);

  size_t record_size = 0;
  std::string ciphertext;

  ASSERT_TRUE(cryptographer.Encrypt(kPlaintext, sender_shared_secret, salt,
                                    &record_size, &ciphertext));

  EXPECT_GT(record_size, 1u);
  EXPECT_EQ(
    sizeof(uint16_t) + strlen(kPlaintext) + 16u /* authentication tag */,
    ciphertext.size());

  // Verify that the created ciphertext matches the reference ciphertext.
  EXPECT_EQ(reference_ciphertext, ciphertext);

  // Decrypt the ciphertext with the default record size to verify that the
  // resulting plaintext matches the input text.
  std::string plaintext;

  ASSERT_TRUE(cryptographer.Decrypt(
      reference_ciphertext, receiver_shared_secret, salt,
      4096 /* record size */, &plaintext));

  // Verify that the decrypted plaintext matches the reference plaintext.
  EXPECT_EQ(kPlaintext, plaintext);
}

}  // namespace gcm
