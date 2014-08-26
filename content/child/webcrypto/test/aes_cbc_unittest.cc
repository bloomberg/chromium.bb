// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "content/child/webcrypto/algorithm_dispatch.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/test/test_helpers.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Creates an AES-CBC algorithm.
blink::WebCryptoAlgorithm CreateAesCbcAlgorithm(
    const std::vector<uint8_t>& iv) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesCbc,
      new blink::WebCryptoAesCbcParams(vector_as_array(&iv), iv.size()));
}

blink::WebCryptoAlgorithm CreateAesCbcKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesCbc,
                                  key_length_bits);
}

blink::WebCryptoKey GetTestAesCbcKey() {
  const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      HexStringToBytes(key_hex),
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

  // Verify exported raw key is identical to the imported data
  std::vector<uint8_t> raw_key;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_BYTES_EQ_HEX(key_hex, raw_key);
  return key;
}

TEST(WebCryptoAesCbcTest, IvTooSmall) {
  std::vector<uint8_t> output;

  // Use an invalid |iv| (fewer than 16 bytes)
  std::vector<uint8_t> input(32);
  std::vector<uint8_t> iv;
  EXPECT_EQ(Status::ErrorIncorrectSizeAesCbcIv(),
            Encrypt(CreateAesCbcAlgorithm(iv),
                    GetTestAesCbcKey(),
                    CryptoData(input),
                    &output));
  EXPECT_EQ(Status::ErrorIncorrectSizeAesCbcIv(),
            Decrypt(CreateAesCbcAlgorithm(iv),
                    GetTestAesCbcKey(),
                    CryptoData(input),
                    &output));
}

TEST(WebCryptoAesCbcTest, IvTooLarge) {
  std::vector<uint8_t> output;

  // Use an invalid |iv| (more than 16 bytes)
  std::vector<uint8_t> input(32);
  std::vector<uint8_t> iv(17);
  EXPECT_EQ(Status::ErrorIncorrectSizeAesCbcIv(),
            Encrypt(CreateAesCbcAlgorithm(iv),
                    GetTestAesCbcKey(),
                    CryptoData(input),
                    &output));
  EXPECT_EQ(Status::ErrorIncorrectSizeAesCbcIv(),
            Decrypt(CreateAesCbcAlgorithm(iv),
                    GetTestAesCbcKey(),
                    CryptoData(input),
                    &output));
}

TEST(WebCryptoAesCbcTest, InputTooLarge) {
  std::vector<uint8_t> output;

  // Give an input that is too large (would cause integer overflow when
  // narrowing to an int). Note that both OpenSSL and NSS operate on signed int
  // lengths.
  std::vector<uint8_t> iv(16);

  // Pretend the input is large. Don't pass data pointer as NULL in case that
  // is special cased; the implementation shouldn't actually dereference the
  // data.
  CryptoData input(&iv[0], INT_MAX - 3);

  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      Encrypt(CreateAesCbcAlgorithm(iv), GetTestAesCbcKey(), input, &output));
  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      Decrypt(CreateAesCbcAlgorithm(iv), GetTestAesCbcKey(), input, &output));
}

TEST(WebCryptoAesCbcTest, KeyTooSmall) {
  std::vector<uint8_t> output;

  // Fail importing the key (too few bytes specified)
  std::vector<uint8_t> key_raw(1);
  std::vector<uint8_t> iv(16);

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  EXPECT_EQ(Status::ErrorImportAesKeyLength(),
            ImportKey(blink::WebCryptoKeyFormatRaw,
                      CryptoData(key_raw),
                      CreateAesCbcAlgorithm(iv),
                      true,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));
}

TEST(WebCryptoAesCbcTest, ExportKeyUnsupportedFormat) {
  std::vector<uint8_t> output;

  // Fail exporting the key in SPKI and PKCS#8 formats (not allowed for secret
  // keys).
  EXPECT_EQ(
      Status::ErrorUnsupportedExportKeyFormat(),
      ExportKey(blink::WebCryptoKeyFormatSpki, GetTestAesCbcKey(), &output));
  EXPECT_EQ(
      Status::ErrorUnsupportedExportKeyFormat(),
      ExportKey(blink::WebCryptoKeyFormatPkcs8, GetTestAesCbcKey(), &output));
}

TEST(WebCryptoAesCbcTest, ImportKeyUnsupportedFormat) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_EQ(Status::ErrorUnsupportedImportKeyFormat(),
            ImportKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                      true,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));
  ASSERT_EQ(Status::ErrorUnsupportedImportKeyFormat(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                      true,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));
}

TEST(WebCryptoAesCbcTest, KnownAnswerEncryptDecrypt) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("aes_cbc.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    std::vector<uint8_t> test_key = GetBytesFromHexString(test, "key");
    std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");
    std::vector<uint8_t> test_plain_text =
        GetBytesFromHexString(test, "plain_text");
    std::vector<uint8_t> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    EXPECT_EQ(test_key.size() * 8, key.algorithm().aesParams()->lengthBits());

    // Verify exported raw key is identical to the imported data
    std::vector<uint8_t> raw_key;
    EXPECT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    EXPECT_BYTES_EQ(test_key, raw_key);

    std::vector<uint8_t> output;

    // Test encryption.
    EXPECT_EQ(Status::Success(),
              Encrypt(CreateAesCbcAlgorithm(test_iv),
                      key,
                      CryptoData(test_plain_text),
                      &output));
    EXPECT_BYTES_EQ(test_cipher_text, output);

    // Test decryption.
    EXPECT_EQ(Status::Success(),
              Decrypt(CreateAesCbcAlgorithm(test_iv),
                      key,
                      CryptoData(test_cipher_text),
                      &output));
    EXPECT_BYTES_EQ(test_plain_text, output);
  }
}

TEST(WebCryptoAesCbcTest, DecryptTruncatedCipherText) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("aes_cbc.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    std::vector<uint8_t> test_key = GetBytesFromHexString(test, "key");
    std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");
    std::vector<uint8_t> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    std::vector<uint8_t> output;

    const unsigned int kAesCbcBlockSize = 16;

    // Decrypt with a padding error by stripping the last block. This also ends
    // up testing decryption over empty cipher text.
    if (test_cipher_text.size() >= kAesCbcBlockSize) {
      EXPECT_EQ(Status::OperationError(),
                Decrypt(CreateAesCbcAlgorithm(test_iv),
                        key,
                        CryptoData(&test_cipher_text[0],
                                   test_cipher_text.size() - kAesCbcBlockSize),
                        &output));
    }

    // Decrypt cipher text which is not a multiple of block size by stripping
    // a few bytes off the cipher text.
    if (test_cipher_text.size() > 3) {
      EXPECT_EQ(
          Status::OperationError(),
          Decrypt(CreateAesCbcAlgorithm(test_iv),
                  key,
                  CryptoData(&test_cipher_text[0], test_cipher_text.size() - 3),
                  &output));
    }
  }
}

// TODO(eroman): Do this same test for AES-GCM, AES-KW, AES-CTR ?
TEST(WebCryptoAesCbcTest, GenerateKeyIsRandom) {
  // Check key generation for each allowed key length.
  std::vector<blink::WebCryptoAlgorithm> algorithm;
  const unsigned short kKeyLength[] = {128, 256};
  for (size_t key_length_i = 0; key_length_i < ARRAYSIZE_UNSAFE(kKeyLength);
       ++key_length_i) {
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

    std::vector<std::vector<uint8_t> > keys;
    std::vector<uint8_t> key_bytes;

    // Generate a small sample of keys.
    for (int j = 0; j < 16; ++j) {
      ASSERT_EQ(Status::Success(),
                GenerateSecretKey(
                    CreateAesCbcKeyGenAlgorithm(kKeyLength[key_length_i]),
                    true,
                    0,
                    &key));
      EXPECT_TRUE(key.handle());
      EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
      ASSERT_EQ(Status::Success(),
                ExportKey(blink::WebCryptoKeyFormatRaw, key, &key_bytes));
      EXPECT_EQ(key_bytes.size() * 8,
                key.algorithm().aesParams()->lengthBits());
      keys.push_back(key_bytes);
    }
    // Ensure all entries in the key sample set are unique. This is a simplistic
    // estimate of whether the generated keys appear random.
    EXPECT_FALSE(CopiesExist(keys));
  }
}

TEST(WebCryptoAesCbcTest, GenerateKeyBadLength) {
  const unsigned short kKeyLen[] = {0, 127, 257};
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kKeyLen); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Status::ErrorGenerateKeyLength(),
              GenerateSecretKey(
                  CreateAesCbcKeyGenAlgorithm(kKeyLen[i]), true, 0, &key));
  }
}

// If key_ops is specified but empty, no key usages are allowed for the key.
TEST(WebCryptoAesCbcTest, ImportKeyJwkEmptyKeyOps) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  dict.Set("key_ops", new base::ListValue);  // Takes ownership.

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           0,
                           &key));

  EXPECT_EQ(0, key.usages());

  // The JWK does not contain encrypt usages.
  EXPECT_EQ(
      Status::ErrorJwkKeyopsInconsistent(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));

  // The JWK does not contain sign usage (nor is it applicable).
  EXPECT_EQ(
      Status::ErrorCreateKeyBadUsages(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageSign,
                           &key));
}

// If key_ops is missing, then any key usages can be specified.
TEST(WebCryptoAesCbcTest, ImportKeyJwkNoKeyOps) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  // The JWK does not contain sign usage (nor is it applicable).
  EXPECT_EQ(
      Status::ErrorCreateKeyBadUsages(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageVerify,
                           &key));
}

TEST(WebCryptoAesCbcTest, ImportKeyJwkKeyOpsEncryptDecrypt) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);  // Takes ownership.

  key_ops->AppendString("encrypt");

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  key_ops->AppendString("decrypt");

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageDecrypt,
                           &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageDecrypt, key.usages());

  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(
          dict,
          CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
          false,
          blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageEncrypt,
          &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
            key.usages());
}

// Test failure if input usage is NOT a strict subset of the JWK usage.
TEST(WebCryptoAesCbcTest, ImportKeyJwkKeyOpsNotSuperset) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);  // Takes ownership.

  key_ops->AppendString("encrypt");

  EXPECT_EQ(
      Status::ErrorJwkKeyopsInconsistent(),
      ImportKeyJwkFromDict(
          dict,
          CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
          false,
          blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
          &key));
}

TEST(WebCryptoAesCbcTest, ImportKeyJwkUseEnc) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  // Test JWK composite use 'enc' usage
  dict.SetString("alg", "A128CBC");
  dict.SetString("use", "enc");
  EXPECT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageDecrypt |
                               blink::WebCryptoKeyUsageEncrypt |
                               blink::WebCryptoKeyUsageWrapKey |
                               blink::WebCryptoKeyUsageUnwrapKey,
                           &key));
  EXPECT_EQ(blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageEncrypt |
                blink::WebCryptoKeyUsageWrapKey |
                blink::WebCryptoKeyUsageUnwrapKey,
            key.usages());
}

TEST(WebCryptoAesCbcTest, ImportJwkInvalidJson) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  // Fail on empty JSON.
  EXPECT_EQ(Status::ErrorImportEmptyKeyData(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(MakeJsonVector("")),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                      false,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));

  // Fail on invalid JSON.
  const std::vector<uint8_t> bad_json_vec = MakeJsonVector(
      "{"
      "\"kty\"         : \"oct\","
      "\"alg\"         : \"HS256\","
      "\"use\"         : ");
  EXPECT_EQ(Status::ErrorJwkNotDictionary(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(bad_json_vec),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                      false,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));
}

// Fail on JWK alg present but incorrect (expecting A128CBC).
TEST(WebCryptoAesCbcTest, ImportJwkIncorrectAlg) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A127CBC");  // Not valid.
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  EXPECT_EQ(
      Status::ErrorJwkAlgorithmInconsistent(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on invalid kty.
TEST(WebCryptoAesCbcTest, ImportJwkInvalidKty) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "foo");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  EXPECT_EQ(
      Status::ErrorJwkUnexpectedKty("oct"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on missing kty.
TEST(WebCryptoAesCbcTest, ImportJwkMissingKty) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  EXPECT_EQ(
      Status::ErrorJwkPropertyMissing("kty"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on kty wrong type.
TEST(WebCryptoAesCbcTest, ImportJwkKtyWrongType) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetDouble("kty", 0.1);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  EXPECT_EQ(
      Status::ErrorJwkPropertyWrongType("kty", "string"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on invalid use.
TEST(WebCryptoAesCbcTest, ImportJwkUnrecognizedUse) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("use", "foo");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  EXPECT_EQ(
      Status::ErrorJwkUnrecognizedUse(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on invalid use (wrong type).
TEST(WebCryptoAesCbcTest, ImportJwkUseWrongType) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetBoolean("use", true);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  EXPECT_EQ(
      Status::ErrorJwkPropertyWrongType("use", "string"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on invalid extractable (wrong type).
TEST(WebCryptoAesCbcTest, ImportJwkExtWrongType) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetInteger("ext", 0);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  EXPECT_EQ(
      Status::ErrorJwkPropertyWrongType("ext", "boolean"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on invalid key_ops (wrong type).
TEST(WebCryptoAesCbcTest, ImportJwkKeyOpsWrongType) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  dict.SetBoolean("key_ops", true);

  EXPECT_EQ(
      Status::ErrorJwkPropertyWrongType("key_ops", "list"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on inconsistent key_ops - asking for "encrypt" however JWK contains
// only "foo".
TEST(WebCryptoAesCbcTest, ImportJwkKeyOpsLacksUsages) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  base::ListValue* key_ops = new base::ListValue;
  // Note: the following call makes dict assume ownership of key_ops.
  dict.Set("key_ops", key_ops);
  key_ops->AppendString("foo");
  EXPECT_EQ(
      Status::ErrorJwkKeyopsInconsistent(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Import a JWK with unrecognized values for "key_ops".
TEST(WebCryptoAesCbcTest, ImportJwkUnrecognizedKeyOps) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A128CBC");
  dict.SetString("use", "enc");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);
  key_ops->AppendString("foo");
  key_ops->AppendString("bar");
  key_ops->AppendString("baz");
  key_ops->AppendString("encrypt");
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, false, usage_mask, &key));
}

// Import a JWK with a value in key_ops array that is not a string.
TEST(WebCryptoAesCbcTest, ImportJwkNonStringKeyOp) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A128CBC");
  dict.SetString("use", "enc");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);
  key_ops->AppendString("encrypt");
  key_ops->AppendInteger(3);
  EXPECT_EQ(Status::ErrorJwkPropertyWrongType("key_ops[1]", "string"),
            ImportKeyJwkFromDict(dict, algorithm, false, usage_mask, &key));
}

// Fail on missing k.
TEST(WebCryptoAesCbcTest, ImportJwkMissingK) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");

  EXPECT_EQ(
      Status::ErrorJwkPropertyMissing("k"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on bad b64 encoding for k.
TEST(WebCryptoAesCbcTest, ImportJwkBadB64ForK) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "Qk3f0DsytU8lfza2au #$% Htaw2xpop9GYyTuH0p5GghxTI=");
  EXPECT_EQ(
      Status::ErrorJwkBase64Decode("k"),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on empty k.
TEST(WebCryptoAesCbcTest, ImportJwkEmptyK) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "");

  EXPECT_EQ(
      Status::ErrorImportAesKeyLength(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on empty k (with alg specified).
TEST(WebCryptoAesCbcTest, ImportJwkEmptyK2) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A128CBC");
  dict.SetString("k", "");

  EXPECT_EQ(
      Status::ErrorJwkIncorrectKeyLength(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on k actual length (120 bits) inconsistent with the embedded JWK alg
// value (128) for an AES key.
TEST(WebCryptoAesCbcTest, ImportJwkInconsistentKLength) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A128CBC");
  dict.SetString("k", "AVj42h0Y5aqGtE3yluKL");
  EXPECT_EQ(
      Status::ErrorJwkIncorrectKeyLength(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// Fail on k actual length (192 bits) inconsistent with the embedded JWK alg
// value (128) for an AES key.
TEST(WebCryptoAesCbcTest, ImportJwkInconsistentKLength2) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A128CBC");
  dict.SetString("k", "dGhpcyAgaXMgIDI0ICBieXRlcyBsb25n");
  EXPECT_EQ(
      Status::ErrorJwkIncorrectKeyLength(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

TEST(WebCryptoAesCbcTest, ImportExportJwk) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);

  // AES-CBC 128
  ImportExportJwkSymmetricKey(
      128,
      algorithm,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      "A128CBC");

  // AES-CBC 256
  ImportExportJwkSymmetricKey(
      256, algorithm, blink::WebCryptoKeyUsageDecrypt, "A256CBC");

  // Large usage value
  ImportExportJwkSymmetricKey(
      256,
      algorithm,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
          blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey,
      "A256CBC");
}

// AES 192-bit is not allowed: http://crbug.com/381829
TEST(WebCryptoAesCbcTest, ImportAesCbc192Raw) {
  std::vector<uint8_t> key_raw(24, 0);
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  Status status = ImportKey(blink::WebCryptoKeyFormatRaw,
                            CryptoData(key_raw),
                            CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                            true,
                            blink::WebCryptoKeyUsageEncrypt,
                            &key);
  ASSERT_EQ(Status::ErrorAes192BitUnsupported(), status);
}

// AES 192-bit is not allowed: http://crbug.com/381829
TEST(WebCryptoAesCbcTest, ImportAesCbc192Jwk) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "A192CBC");
  dict.SetString("k", "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh");

  EXPECT_EQ(
      Status::ErrorAes192BitUnsupported(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           false,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
}

// AES 192-bit is not allowed: http://crbug.com/381829
TEST(WebCryptoAesCbcTest, GenerateAesCbc192) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  Status status = GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(192),
                                    true,
                                    blink::WebCryptoKeyUsageEncrypt,
                                    &key);
  ASSERT_EQ(Status::ErrorAes192BitUnsupported(), status);
}

// AES 192-bit is not allowed: http://crbug.com/381829
TEST(WebCryptoAesCbcTest, UnwrapAesCbc192) {
  std::vector<uint8_t> wrapping_key_data(16, 0);
  std::vector<uint8_t> wrapped_key = HexStringToBytes(
      "1A07ACAB6C906E50883173C29441DB1DE91D34F45C435B5F99C822867FB3956F");

  blink::WebCryptoKey wrapping_key =
      ImportSecretKeyFromRaw(wrapping_key_data,
                             CreateAlgorithm(blink::WebCryptoAlgorithmIdAesKw),
                             blink::WebCryptoKeyUsageUnwrapKey);

  blink::WebCryptoKey unwrapped_key = blink::WebCryptoKey::createNull();
  ASSERT_EQ(Status::ErrorAes192BitUnsupported(),
            UnwrapKey(blink::WebCryptoKeyFormatRaw,
                      CryptoData(wrapped_key),
                      wrapping_key,
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesKw),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                      true,
                      blink::WebCryptoKeyUsageEncrypt,
                      &unwrapped_key));
}

// Try importing an AES-CBC key with unsupported key usages using raw
// format. AES-CBC keys support the following usages:
//   'encrypt', 'decrypt', 'wrapKey', 'unwrapKey'
TEST(WebCryptoAesCbcTest, ImportKeyBadUsage_Raw) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);

  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageSign,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageDecrypt,
      blink::WebCryptoKeyUsageDeriveBits,
      blink::WebCryptoKeyUsageUnwrapKey | blink::WebCryptoKeyUsageVerify,
  };

  std::vector<uint8_t> key_bytes(16);

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKey(blink::WebCryptoKeyFormatRaw,
                        CryptoData(key_bytes),
                        algorithm,
                        true,
                        bad_usages[i],
                        &key));
  }
}

// Generate an AES-CBC key with invalid usages. AES-CBC supports:
//   'encrypt', 'decrypt', 'wrapKey', 'unwrapKey'
TEST(WebCryptoAesCbcTest, GenerateKeyBadUsages) {
  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageSign, blink::WebCryptoKeyUsageVerify,
      blink::WebCryptoKeyUsageDecrypt | blink::WebCryptoKeyUsageVerify,
  };

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              GenerateSecretKey(
                  CreateAesCbcKeyGenAlgorithm(128), true, bad_usages[i], &key));
  }
}

// Generate an AES-CBC key and an RSA key pair. Use the AES-CBC key to wrap the
// key pair (using SPKI format for public key, PKCS8 format for private key).
// Then unwrap the wrapped key pair and verify that the key data is the same.
TEST(WebCryptoAesCbcTest, WrapUnwrapRoundtripSpkiPkcs8) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  // Generate the wrapping key.
  blink::WebCryptoKey wrapping_key = blink::WebCryptoKey::createNull();
  ASSERT_EQ(Status::Success(),
            GenerateSecretKey(CreateAesCbcKeyGenAlgorithm(128),
                              true,
                              blink::WebCryptoKeyUsageWrapKey |
                                  blink::WebCryptoKeyUsageUnwrapKey,
                              &wrapping_key));

  // Generate an RSA key pair to be wrapped.
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::WebCryptoAlgorithmIdSha256,
                                modulus_length,
                                public_exponent),
                            true,
                            0,
                            &public_key,
                            &private_key));

  // Export key pair as SPKI + PKCS8
  std::vector<uint8_t> public_key_spki;
  ASSERT_EQ(
      Status::Success(),
      ExportKey(blink::WebCryptoKeyFormatSpki, public_key, &public_key_spki));

  std::vector<uint8_t> private_key_pkcs8;
  ASSERT_EQ(
      Status::Success(),
      ExportKey(
          blink::WebCryptoKeyFormatPkcs8, private_key, &private_key_pkcs8));

  // Wrap the key pair.
  blink::WebCryptoAlgorithm wrap_algorithm =
      CreateAesCbcAlgorithm(std::vector<uint8_t>(16, 0));

  std::vector<uint8_t> wrapped_public_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::WebCryptoKeyFormatSpki,
                    public_key,
                    wrapping_key,
                    wrap_algorithm,
                    &wrapped_public_key));

  std::vector<uint8_t> wrapped_private_key;
  ASSERT_EQ(Status::Success(),
            WrapKey(blink::WebCryptoKeyFormatPkcs8,
                    private_key,
                    wrapping_key,
                    wrap_algorithm,
                    &wrapped_private_key));

  // Unwrap the key pair.
  blink::WebCryptoAlgorithm rsa_import_algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256);

  blink::WebCryptoKey unwrapped_public_key = blink::WebCryptoKey::createNull();

  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(wrapped_public_key),
                      wrapping_key,
                      wrap_algorithm,
                      rsa_import_algorithm,
                      true,
                      0,
                      &unwrapped_public_key));

  blink::WebCryptoKey unwrapped_private_key = blink::WebCryptoKey::createNull();

  ASSERT_EQ(Status::Success(),
            UnwrapKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(wrapped_private_key),
                      wrapping_key,
                      wrap_algorithm,
                      rsa_import_algorithm,
                      true,
                      0,
                      &unwrapped_private_key));

  // Export unwrapped key pair as SPKI + PKCS8
  std::vector<uint8_t> unwrapped_public_key_spki;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatSpki,
                      unwrapped_public_key,
                      &unwrapped_public_key_spki));

  std::vector<uint8_t> unwrapped_private_key_pkcs8;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatPkcs8,
                      unwrapped_private_key,
                      &unwrapped_private_key_pkcs8));

  EXPECT_EQ(public_key_spki, unwrapped_public_key_spki);
  EXPECT_EQ(private_key_pkcs8, unwrapped_private_key_pkcs8);

  EXPECT_NE(public_key_spki, wrapped_public_key);
  EXPECT_NE(private_key_pkcs8, wrapped_private_key);
}

}  // namespace

}  // namespace webcrypto

}  // namespace content
