// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <stddef.h>

#include <memory>

#include "base/base64url.h"
#include "base/macros.h"
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
    std::unique_ptr<crypto::SymmetricKey> random_key(
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
        new GCMMessageCryptographer(local_public_key, peer_public_key,
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
  std::unique_ptr<GCMMessageCryptographer> cryptographer_;

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

  GCMMessageCryptographer hello_cryptographer(public_key, public_key, "Hello");

  GCMMessageCryptographer world_cryptographer(public_key, public_key, "World");

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

// Common infrastructure for reference tests against the examples in the draft:
// https://tools.ietf.org/html/draft-thomson-http-encryption
class GCMMessageCryptographerReferenceTest
    : public GCMMessageCryptographerTest {
 protected:
  // Computes the shared secret between the sender and the receiver. The sender
  // must have a key-pair containing a X.509 SubjectPublicKeyInfo block and a
  // ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo block, whereas the receiver
  // must have a public key in uncompressed EC point format.
  void ComputeSharedSecret(const char* encoded_sender_private_key,
                           const char* encoded_sender_public_key_x509,
                           const char* encoded_receiver_public_key,
                           std::string* shared_secret) const {
    std::string sender_private_key, sender_public_key_x509, receiver_public_key;
    ASSERT_TRUE(base::Base64UrlDecode(
        encoded_sender_private_key,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &sender_private_key));
    ASSERT_TRUE(base::Base64UrlDecode(
        encoded_sender_public_key_x509,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &sender_public_key_x509));
    ASSERT_TRUE(base::Base64UrlDecode(
        encoded_receiver_public_key,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &receiver_public_key));

    ASSERT_TRUE(ComputeSharedP256Secret(
        sender_private_key, sender_public_key_x509, receiver_public_key,
        shared_secret));
  }

  // Creates a new cryptographer based on the P-256 curve with the given public
  // keys of the sender and receiver, and optionally, the authentication secret.
  // The public keys must be given as uncompressed P-256 EC points.
  void CreateCryptographer(
      const char* encoded_receiver_public_key,
      const char* encoded_sender_public_key,
      const char* encoded_auth_secret,
      std::unique_ptr<GCMMessageCryptographer>* cryptographer) const {
    std::string receiver_public_key, sender_public_key, auth_secret;
    ASSERT_TRUE(base::Base64UrlDecode(
        encoded_receiver_public_key,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &receiver_public_key));
    ASSERT_TRUE(base::Base64UrlDecode(
        encoded_sender_public_key,
        base::Base64UrlDecodePolicy::IGNORE_PADDING, &sender_public_key));

    if (encoded_auth_secret) {
      ASSERT_TRUE(base::Base64UrlDecode(
          encoded_auth_secret,
          base::Base64UrlDecodePolicy::IGNORE_PADDING, &auth_secret));
    }

    std::unique_ptr<GCMMessageCryptographer> instance(
        new GCMMessageCryptographer(receiver_public_key, sender_public_key,
                                    auth_secret));

    if (auth_secret.empty())
      instance->set_allow_empty_auth_secret_for_tests(true);

    cryptographer->swap(instance);
  }
};

TEST_F(GCMMessageCryptographerReferenceTest, WithAuthSecret) {
  // The 16-byte salt unique to the message.
  const char kSalt[] = "lngarbyKfMoi9Z75xYXmkg";

  // The 16-byte prearranged secret between the sender and receiver.
  const char kAuthSecret[] = "R29vIGdvbyBnJyBqb29iIQ";

  // The keying material used by the sender to encrypt the |kCiphertext|.
  const char kSenderPrivate[] =
      "MIGxMBwGCiqGSIb3DQEMAQMwDgQIh9aZ3UvuDloCAggABIGQZ-T8CJZe-no4mOTDgX1Gm986"
      "Gsbe3mjJeABhA4KOmut_qJh5kt_DLqdNShiQr-afk3AdkX-fxLZdrcHiW9aWvBjnMAY65zg5"
      "oHsuUaoEuG88Ksbku2u193OENWTQTsYaYE2O44qmRfsX773UNVcWXg_omwIbhbgf6tLZUZH_"
      "dTC3YjzuxjbSP89HPEJ-eBXA";
  const char kSenderPublicUncompressed[] =
      "BNoRDbb84JGm8g5Z5CFxurSqsXWJ11ItfXEWYVLE85Y7CYkDjXsIEc4aqxYaQ1G8BqkXCJ6D"
      "PpDrWtdWj_mugHU";
  const char kSenderPublicX509[] =
      "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE2hENtvzgkabyDlnkIXG6tKqxdYnXUi19cRZh"
      "UsTzljsJiQONewgRzhqrFhpDUbwGqRcInoM-kOta11aP-a6AdQ";

  // The keying material used by the client to decrypt the |kCiphertext|.
  const char kReceiverPrivate[] =
      "MIGxMBwGCiqGSIb3DQEMAQMwDgQIqMt4d7uJdt4CAggABIGQeikRHE3CqUeF-uUtJno9BL0g"
      "mNRyDihZe8P3nF_g-NYVzvdQowsXfYeza6OQOdDuMXxnGgNToVy2jsiWVN6rxCaSMTY622y8"
      "ajW5voSdqC2PakQ8ZNTPNHarLDMC9NpgGKrUh8hfRLhvb7vtbKIWmx-22rQB5yTYdqzN2m7A"
      "GHMWRnVk0mMzMsMjZqYFaa2D";
  const char kReceiverPublicUncompressed[] =
      "BCEkBjzL8Z3C-oi2Q7oE5t2Np-p7osjGLg93qUP0wvqRT21EEWyf0cQDQcakQMqz4hQKYOQ3"
      "il2nNZct4HgAUQU";
  const char kReceiverPublicX509[] =
      "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEISQGPMvxncL6iLZDugTm3Y2n6nuiyMYuD3ep"
      "Q_TC-pFPbUQRbJ_RxANBxqRAyrPiFApg5DeKXac1ly3geABRBQ";

  // The ciphertext and associated plaintext of the message.
  const char kCiphertext[] = "6nqAQUME8hNqw5J3kl8cpVVJylXKYqZOeseZG8UueKpA";
  const char kPlaintext[] = "I am the walrus";

  std::string sender_shared_secret, receiver_shared_secret;

  // Compute the shared secrets between the sender and receiver's keys.
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(
      kSenderPrivate, kSenderPublicX509, kReceiverPublicUncompressed,
      &sender_shared_secret));
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(
      kReceiverPrivate, kReceiverPublicX509, kSenderPublicUncompressed,
      &receiver_shared_secret));

  ASSERT_GT(sender_shared_secret.size(), 0u);
  ASSERT_EQ(sender_shared_secret, receiver_shared_secret);

  std::unique_ptr<GCMMessageCryptographer> cryptographer;
  ASSERT_NO_FATAL_FAILURE(CreateCryptographer(
      kReceiverPublicUncompressed, kSenderPublicUncompressed, kAuthSecret,
      &cryptographer));

  std::string salt;
  ASSERT_TRUE(base::Base64UrlDecode(
        kSalt, base::Base64UrlDecodePolicy::IGNORE_PADDING, &salt));

  std::string encoded_ciphertext, ciphertext, plaintext;
  size_t record_size = 0;

  // Verify that encrypting |kPlaintext| yields the expected |kCiphertext|.
  ASSERT_TRUE(cryptographer->Encrypt(kPlaintext, sender_shared_secret, salt,
                                     &record_size, &ciphertext));

  base::Base64UrlEncode(ciphertext, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_ciphertext);
  ASSERT_EQ(kCiphertext, encoded_ciphertext);

  // Verify that decrypting |kCiphertext| yields the expected |kPlaintext|.
  ASSERT_TRUE(cryptographer->Decrypt(ciphertext, sender_shared_secret, salt,
                                     record_size, &plaintext));
  ASSERT_EQ(kPlaintext, plaintext);
}

TEST_F(GCMMessageCryptographerReferenceTest, WithoutAuthSecret) {
  // The 16-byte salt unique to the message.
  const char kSalt[] = "Qg61ZJRva_XBE9IEUelU3A";

  // The keying material used by the sender to encrypt the |kCiphertext|.
  const char kSenderPrivate[] =
      "MIGxMBwGCiqGSIb3DQEMAQMwDgQIFfJ62c9VwXgCAggABIGQkRxDRPQjwuWp1C3-z1pYTDqF"
      "_NZ1kbPsjmkC3JSv02oAYHtBAtKa2e3oAPqsPfCvoCJBJs6G4WY4EuEO1YFL6RKpNl3DpIUc"
      "v9ShR27p_je_nyLpNBAxn2drnjlF_K6s4gcJmcvCxuNjAwOlLMPvQqGjOR2K_oMs1Hdq0EKJ"
      "NwWt3WUVEpuQF_WhYjCVIeGO";
  const char kSenderPublicUncompressed[] =
      "BDgpRKok2GZZDmS4r63vbJSUtcQx4Fq1V58-6-3NbZzSTlZsQiCEDTQy3CZ0ZMsqeqsEb7qW"
      "2blQHA4S48fynTk";
  const char kSenderPublicX509[] =
      "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEOClEqiTYZlkOZLivre9slJS1xDHgWrVXnz7r"
      "7c1tnNJOVmxCIIQNNDLcJnRkyyp6qwRvupbZuVAcDhLjx_KdOQ";

  // The keying material used by the client to decrypt the |kCiphertext|.
  const char kReceiverPrivate[] =
      "MIGxMBwGCiqGSIb3DQEMAQMwDgQIqMt4d7uJdt4CAggABIGQeikRHE3CqUeF-uUtJno9BL0g"
      "mNRyDihZe8P3nF_g-NYVzvdQowsXfYeza6OQOdDuMXxnGgNToVy2jsiWVN6rxCaSMTY622y8"
      "ajW5voSdqC2PakQ8ZNTPNHarLDMC9NpgGKrUh8hfRLhvb7vtbKIWmx-22rQB5yTYdqzN2m7A"
      "GHMWRnVk0mMzMsMjZqYFaa2D";
  const char kReceiverPublicUncompressed[] =
      "BCEkBjzL8Z3C-oi2Q7oE5t2Np-p7osjGLg93qUP0wvqRT21EEWyf0cQDQcakQMqz4hQKYOQ3"
      "il2nNZct4HgAUQU";
  const char kReceiverPublicX509[] =
      "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEISQGPMvxncL6iLZDugTm3Y2n6nuiyMYuD3ep"
      "Q_TC-pFPbUQRbJ_RxANBxqRAyrPiFApg5DeKXac1ly3geABRBQ";

  // The ciphertext and associated plaintext of the message.
  const char kCiphertext[] = "yqD2bapcx14XxUbtwjiGx69eHE3Yd6AqXcwBpT2Kd1uy";
  const char kPlaintext[] = "I am the walrus";

  std::string sender_shared_secret, receiver_shared_secret;

  // Compute the shared secrets between the sender and receiver's keys.
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(
      kSenderPrivate, kSenderPublicX509, kReceiverPublicUncompressed,
      &sender_shared_secret));
  ASSERT_NO_FATAL_FAILURE(ComputeSharedSecret(
      kReceiverPrivate, kReceiverPublicX509, kSenderPublicUncompressed,
      &receiver_shared_secret));

  ASSERT_GT(sender_shared_secret.size(), 0u);
  ASSERT_EQ(sender_shared_secret, receiver_shared_secret);

  std::unique_ptr<GCMMessageCryptographer> cryptographer;
  ASSERT_NO_FATAL_FAILURE(CreateCryptographer(
      kReceiverPublicUncompressed, kSenderPublicUncompressed,
      nullptr /* auth_secret */, &cryptographer));

  std::string salt;
  ASSERT_TRUE(base::Base64UrlDecode(
        kSalt, base::Base64UrlDecodePolicy::IGNORE_PADDING, &salt));

  std::string encoded_ciphertext, ciphertext, plaintext;
  size_t record_size = 0;

  // Verify that encrypting |kPlaintext| yields the expected |kCiphertext|.
  ASSERT_TRUE(cryptographer->Encrypt(kPlaintext, sender_shared_secret, salt,
                                     &record_size, &ciphertext));

  base::Base64UrlEncode(ciphertext, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_ciphertext);
  ASSERT_EQ(kCiphertext, encoded_ciphertext);

  // Verify that decrypting |kCiphertext| yields the expected |kPlaintext|.
  ASSERT_TRUE(cryptographer->Decrypt(ciphertext, sender_shared_secret, salt,
                                     record_size, &plaintext));
  ASSERT_EQ(kPlaintext, plaintext);
}

}  // namespace gcm
