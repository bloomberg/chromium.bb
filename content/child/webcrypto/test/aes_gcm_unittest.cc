// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "content/child/webcrypto/algorithm_dispatch.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/test/test_helpers.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Creates an AES-GCM algorithm.
blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& additional_data,
    unsigned int tag_length_bits) {
  EXPECT_TRUE(SupportsAesGcm());
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesGcm,
      new blink::WebCryptoAesGcmParams(vector_as_array(&iv),
                                       iv.size(),
                                       true,
                                       vector_as_array(&additional_data),
                                       additional_data.size(),
                                       true,
                                       tag_length_bits));
}

blink::WebCryptoAlgorithm CreateAesGcmKeyGenAlgorithm(
    unsigned short key_length_bits) {
  EXPECT_TRUE(SupportsAesGcm());
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesGcm,
                                  key_length_bits);
}

Status AesGcmEncrypt(const blink::WebCryptoKey& key,
                     const std::vector<uint8_t>& iv,
                     const std::vector<uint8_t>& additional_data,
                     unsigned int tag_length_bits,
                     const std::vector<uint8_t>& plain_text,
                     std::vector<uint8_t>* cipher_text,
                     std::vector<uint8_t>* authentication_tag) {
  EXPECT_TRUE(SupportsAesGcm());
  blink::WebCryptoAlgorithm algorithm =
      CreateAesGcmAlgorithm(iv, additional_data, tag_length_bits);

  std::vector<uint8_t> output;
  Status status = Encrypt(algorithm, key, CryptoData(plain_text), &output);
  if (status.IsError())
    return status;

  if ((tag_length_bits % 8) != 0) {
    EXPECT_TRUE(false) << "Encrypt should have failed.";
    return Status::OperationError();
  }

  size_t tag_length_bytes = tag_length_bits / 8;

  if (tag_length_bytes > output.size()) {
    EXPECT_TRUE(false) << "tag length is larger than output";
    return Status::OperationError();
  }

  // The encryption result is cipher text with authentication tag appended.
  cipher_text->assign(output.begin(),
                      output.begin() + (output.size() - tag_length_bytes));
  authentication_tag->assign(output.begin() + cipher_text->size(),
                             output.end());

  return Status::Success();
}

Status AesGcmDecrypt(const blink::WebCryptoKey& key,
                     const std::vector<uint8_t>& iv,
                     const std::vector<uint8_t>& additional_data,
                     unsigned int tag_length_bits,
                     const std::vector<uint8_t>& cipher_text,
                     const std::vector<uint8_t>& authentication_tag,
                     std::vector<uint8_t>* plain_text) {
  EXPECT_TRUE(SupportsAesGcm());
  blink::WebCryptoAlgorithm algorithm =
      CreateAesGcmAlgorithm(iv, additional_data, tag_length_bits);

  // Join cipher text and authentication tag.
  std::vector<uint8_t> cipher_text_with_tag;
  cipher_text_with_tag.reserve(cipher_text.size() + authentication_tag.size());
  cipher_text_with_tag.insert(
      cipher_text_with_tag.end(), cipher_text.begin(), cipher_text.end());
  cipher_text_with_tag.insert(cipher_text_with_tag.end(),
                              authentication_tag.begin(),
                              authentication_tag.end());

  return Decrypt(algorithm, key, CryptoData(cipher_text_with_tag), plain_text);
}

TEST(WebCryptoAesGcmTest, GenerateKeyBadLength) {
  if (!SupportsAesGcm()) {
    LOG(WARNING) << "AES GCM not supported, skipping tests";
    return;
  }

  const unsigned short kKeyLen[] = {0, 127, 257};
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kKeyLen); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Status::ErrorGenerateKeyLength(),
              GenerateSecretKey(
                  CreateAesGcmKeyGenAlgorithm(kKeyLen[i]), true, 0, &key));
  }
}

TEST(WebCryptoAesGcmTest, ImportExportJwk) {
  // Some Linux test runners may not have a new enough version of NSS.
  if (!SupportsAesGcm()) {
    LOG(WARNING) << "AES GCM not supported, skipping tests";
    return;
  }

  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesGcm);

  // AES-GCM 128
  ImportExportJwkSymmetricKey(
      128,
      algorithm,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      "A128GCM");

  // AES-GCM 256
  ImportExportJwkSymmetricKey(
      256, algorithm, blink::WebCryptoKeyUsageDecrypt, "A256GCM");
}

// TODO(eroman):
//   * Test decryption when the tag length exceeds input size
//   * Test decryption with empty input
//   * Test decryption with tag length of 0.
TEST(WebCryptoAesGcmTest, SampleSets) {
  // Some Linux test runners may not have a new enough version of NSS.
  if (!SupportsAesGcm()) {
    LOG(WARNING) << "AES GCM not supported, skipping tests";
    return;
  }

  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("aes_gcm.json", &tests));

  // Note that WebCrypto appends the authentication tag to the ciphertext.
  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    const std::vector<uint8_t> test_key = GetBytesFromHexString(test, "key");
    const std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");
    const std::vector<uint8_t> test_additional_data =
        GetBytesFromHexString(test, "additional_data");
    const std::vector<uint8_t> test_plain_text =
        GetBytesFromHexString(test, "plain_text");
    const std::vector<uint8_t> test_authentication_tag =
        GetBytesFromHexString(test, "authentication_tag");
    const unsigned int test_tag_size_bits = test_authentication_tag.size() * 8;
    const std::vector<uint8_t> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        CreateAlgorithm(blink::WebCryptoAlgorithmIdAesGcm),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    // Verify exported raw key is identical to the imported data
    std::vector<uint8_t> raw_key;
    EXPECT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));

    EXPECT_BYTES_EQ(test_key, raw_key);

    // Test encryption.
    std::vector<uint8_t> cipher_text;
    std::vector<uint8_t> authentication_tag;
    EXPECT_EQ(Status::Success(),
              AesGcmEncrypt(key,
                            test_iv,
                            test_additional_data,
                            test_tag_size_bits,
                            test_plain_text,
                            &cipher_text,
                            &authentication_tag));

    EXPECT_BYTES_EQ(test_cipher_text, cipher_text);
    EXPECT_BYTES_EQ(test_authentication_tag, authentication_tag);

    // Test decryption.
    std::vector<uint8_t> plain_text;
    EXPECT_EQ(Status::Success(),
              AesGcmDecrypt(key,
                            test_iv,
                            test_additional_data,
                            test_tag_size_bits,
                            test_cipher_text,
                            test_authentication_tag,
                            &plain_text));
    EXPECT_BYTES_EQ(test_plain_text, plain_text);

    // Decryption should fail if any of the inputs are tampered with.
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key,
                            Corrupted(test_iv),
                            test_additional_data,
                            test_tag_size_bits,
                            test_cipher_text,
                            test_authentication_tag,
                            &plain_text));
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key,
                            test_iv,
                            Corrupted(test_additional_data),
                            test_tag_size_bits,
                            test_cipher_text,
                            test_authentication_tag,
                            &plain_text));
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key,
                            test_iv,
                            test_additional_data,
                            test_tag_size_bits,
                            Corrupted(test_cipher_text),
                            test_authentication_tag,
                            &plain_text));
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key,
                            test_iv,
                            test_additional_data,
                            test_tag_size_bits,
                            test_cipher_text,
                            Corrupted(test_authentication_tag),
                            &plain_text));

    // Try different incorrect tag lengths
    uint8_t kAlternateTagLengths[] = {0, 8, 96, 120, 128, 160, 255};
    for (size_t tag_i = 0; tag_i < arraysize(kAlternateTagLengths); ++tag_i) {
      unsigned int wrong_tag_size_bits = kAlternateTagLengths[tag_i];
      if (test_tag_size_bits == wrong_tag_size_bits)
        continue;
      EXPECT_NE(Status::Success(),
                AesGcmDecrypt(key,
                              test_iv,
                              test_additional_data,
                              wrong_tag_size_bits,
                              test_cipher_text,
                              test_authentication_tag,
                              &plain_text));
    }
  }
}

}  // namespace

}  // namespace webcrypto

}  // namespace content
