// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
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

// Creates an HMAC algorithm whose parameters struct is compatible with key
// generation. It is an error to call this with a hash_id that is not a SHA*.
// The key_length_bits parameter is optional, with zero meaning unspecified.
blink::WebCryptoAlgorithm CreateHmacKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned int key_length_bits) {
  DCHECK(blink::WebCryptoAlgorithm::isHash(hash_id));
  // key_length_bytes == 0 means unspecified
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacKeyGenParams(
          CreateAlgorithm(hash_id), (key_length_bits != 0), key_length_bits));
}

TEST(WebCryptoHmacTest, HMACSampleSets) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("hmac.json", &tests));
  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoAlgorithm test_hash = GetDigestAlgorithm(test, "hash");
    const std::vector<uint8_t> test_key = GetBytesFromHexString(test, "key");
    const std::vector<uint8_t> test_message =
        GetBytesFromHexString(test, "message");
    const std::vector<uint8_t> test_mac = GetBytesFromHexString(test, "mac");

    blink::WebCryptoAlgorithm algorithm =
        CreateAlgorithm(blink::WebCryptoAlgorithmIdHmac);

    blink::WebCryptoAlgorithm import_algorithm =
        CreateHmacImportAlgorithm(test_hash.id());

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        import_algorithm,
        blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify);

    EXPECT_EQ(test_hash.id(), key.algorithm().hmacParams()->hash().id());
    EXPECT_EQ(test_key.size() * 8, key.algorithm().hmacParams()->lengthBits());

    // Verify exported raw key is identical to the imported data
    std::vector<uint8_t> raw_key;
    EXPECT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    EXPECT_BYTES_EQ(test_key, raw_key);

    std::vector<uint8_t> output;

    ASSERT_EQ(Status::Success(),
              Sign(algorithm, key, CryptoData(test_message), &output));

    EXPECT_BYTES_EQ(test_mac, output);

    bool signature_match = false;
    EXPECT_EQ(Status::Success(),
              Verify(algorithm,
                     key,
                     CryptoData(output),
                     CryptoData(test_message),
                     &signature_match));
    EXPECT_TRUE(signature_match);

    // Ensure truncated signature does not verify by passing one less byte.
    EXPECT_EQ(Status::Success(),
              Verify(algorithm,
                     key,
                     CryptoData(vector_as_array(&output), output.size() - 1),
                     CryptoData(test_message),
                     &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure truncated signature does not verify by passing no bytes.
    EXPECT_EQ(Status::Success(),
              Verify(algorithm,
                     key,
                     CryptoData(),
                     CryptoData(test_message),
                     &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure extra long signature does not cause issues and fails.
    const unsigned char kLongSignature[1024] = {0};
    EXPECT_EQ(Status::Success(),
              Verify(algorithm,
                     key,
                     CryptoData(kLongSignature, sizeof(kLongSignature)),
                     CryptoData(test_message),
                     &signature_match));
    EXPECT_FALSE(signature_match);
  }
}

TEST(WebCryptoHmacTest, GenerateKeyIsRandom) {
  // Generate a small sample of HMAC keys.
  std::vector<std::vector<uint8_t> > keys;
  for (int i = 0; i < 16; ++i) {
    std::vector<uint8_t> key_bytes;
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    blink::WebCryptoAlgorithm algorithm =
        CreateHmacKeyGenAlgorithm(blink::WebCryptoAlgorithmIdSha1, 512);
    ASSERT_EQ(Status::Success(), GenerateSecretKey(algorithm, true, 0, &key));
    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());
    EXPECT_EQ(blink::WebCryptoAlgorithmIdSha1,
              key.algorithm().hmacParams()->hash().id());
    EXPECT_EQ(512u, key.algorithm().hmacParams()->lengthBits());

    std::vector<uint8_t> raw_key;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    EXPECT_EQ(64U, raw_key.size());
    keys.push_back(raw_key);
  }
  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(keys));
}

// If the key length is not provided, then the block size is used.
TEST(WebCryptoHmacTest, GenerateKeyNoLengthSha1) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacKeyGenAlgorithm(blink::WebCryptoAlgorithmIdSha1, 0);
  ASSERT_EQ(Status::Success(), GenerateSecretKey(algorithm, true, 0, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha1,
            key.algorithm().hmacParams()->hash().id());
  EXPECT_EQ(512u, key.algorithm().hmacParams()->lengthBits());
  std::vector<uint8_t> raw_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_EQ(64U, raw_key.size());
}

// If the key length is not provided, then the block size is used.
TEST(WebCryptoHmacTest, GenerateKeyNoLengthSha512) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacKeyGenAlgorithm(blink::WebCryptoAlgorithmIdSha512, 0);
  ASSERT_EQ(Status::Success(), GenerateSecretKey(algorithm, true, 0, &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha512,
            key.algorithm().hmacParams()->hash().id());
  EXPECT_EQ(1024u, key.algorithm().hmacParams()->lengthBits());
  std::vector<uint8_t> raw_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_EQ(128U, raw_key.size());
}

TEST(WebCryptoHmacTest, ImportKeyJwkKeyOpsSignVerify) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);  // Takes ownership.

  key_ops->AppendString("sign");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict,
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256),
                false,
                blink::WebCryptoKeyUsageSign,
                &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageSign, key.usages());

  key_ops->AppendString("verify");

  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict,
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256),
                false,
                blink::WebCryptoKeyUsageVerify,
                &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
}

// Test 'use' inconsistent with 'key_ops'.
TEST(WebCryptoHmacTest, ImportKeyJwkUseInconsisteWithKeyOps) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
  base::ListValue* key_ops = new base::ListValue;
  dict.Set("key_ops", key_ops);  // Takes ownership.

  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  key_ops->AppendString("sign");
  key_ops->AppendString("verify");
  key_ops->AppendString("encrypt");
  EXPECT_EQ(Status::ErrorJwkUseAndKeyopsInconsistent(),
            ImportKeyJwkFromDict(
                dict,
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256),
                false,
                blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
                &key));
}

// Test JWK composite 'sig' use
TEST(WebCryptoHmacTest, ImportKeyJwkUseSig) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");

  dict.SetString("use", "sig");
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict,
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256),
                false,
                blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
                &key));

  EXPECT_EQ(blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
            key.usages());
}

TEST(WebCryptoHmacTest, ImportJwkInputConsistency) {
  // The Web Crypto spec says that if a JWK value is present, but is
  // inconsistent with the input value, the operation must fail.

  // Consistency rules when JWK value is not present: Inputs should be used.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageVerify;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  std::vector<uint8_t> json_vec = MakeJsonVector(dict);
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      algorithm,
                      extractable,
                      usage_mask,
                      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  EXPECT_EQ(extractable, key.extractable());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            key.algorithm().hmacParams()->hash().id());
  EXPECT_EQ(320u, key.algorithm().hmacParams()->lengthBits());
  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
  key = blink::WebCryptoKey::createNull();

  // Consistency rules when JWK value exists: Fail if inconsistency is found.

  // Pass: All input values are consistent with the JWK values.
  dict.Clear();
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  json_vec = MakeJsonVector(dict);
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      algorithm,
                      extractable,
                      usage_mask,
                      &key));

  // Extractable cases:
  // 1. input=T, JWK=F ==> fail (inconsistent)
  // 4. input=F, JWK=F ==> pass, result extractable is F
  // 2. input=T, JWK=T ==> pass, result extractable is T
  // 3. input=F, JWK=T ==> pass, result extractable is F
  EXPECT_EQ(Status::ErrorJwkExtInconsistent(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      algorithm,
                      true,
                      usage_mask,
                      &key));
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      algorithm,
                      false,
                      usage_mask,
                      &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("ext", true);
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, true, usage_mask, &key));
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("ext", true);  // restore previous value

  // Fail: Input algorithm (AES-CBC) is inconsistent with JWK value
  // (HMAC SHA256).
  dict.Clear();
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  EXPECT_EQ(
      Status::ErrorJwkAlgorithmInconsistent(),
      ImportKeyJwkFromDict(dict,
                           CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                           extractable,
                           blink::WebCryptoKeyUsageEncrypt,
                           &key));
  // Fail: Input usage (encrypt) is inconsistent with JWK value (use=sig).
  EXPECT_EQ(Status::ErrorJwkUseInconsistent(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
                      extractable,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));

  // Fail: Input algorithm (HMAC SHA1) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_EQ(
      Status::ErrorJwkAlgorithmInconsistent(),
      ImportKey(blink::WebCryptoKeyFormatJwk,
                CryptoData(json_vec),
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha1),
                extractable,
                usage_mask,
                &key));

  // Pass: JWK alg missing but input algorithm specified: use input value
  dict.Remove("alg", NULL);
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(
                dict,
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256),
                extractable,
                usage_mask,
                &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());
  dict.SetString("alg", "HS256");

  // Fail: Input usage_mask (encrypt) is not a subset of the JWK value
  // (sign|verify). Moreover "encrypt" is not a valid usage for HMAC.
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      algorithm,
                      extractable,
                      blink::WebCryptoKeyUsageEncrypt,
                      &key));

  // Fail: Input usage_mask (encrypt|sign|verify) is not a subset of the JWK
  // value (sign|verify). Moreover "encrypt" is not a valid usage for HMAC.
  usage_mask = blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageSign |
               blink::WebCryptoKeyUsageVerify;
  EXPECT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json_vec),
                      algorithm,
                      extractable,
                      usage_mask,
                      &key));

  // TODO(padolph): kty vs alg consistency tests: Depending on the kty value,
  // only certain alg values are permitted. For example, when kty = "RSA" alg
  // must be of the RSA family, or when kty = "oct" alg must be symmetric
  // algorithm.

  // TODO(padolph): key_ops consistency tests
}

TEST(WebCryptoHmacTest, ImportJwkHappy) {
  // This test verifies the happy path of JWK import, including the application
  // of the imported key material.

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageSign;

  // Import a symmetric key JWK and HMAC-SHA256 sign()
  // Uses the first SHA256 test vector from the HMAC sample set above.

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  dict.SetBoolean("ext", false);
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");

  ASSERT_EQ(
      Status::Success(),
      ImportKeyJwkFromDict(dict, algorithm, extractable, usage_mask, &key));

  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            key.algorithm().hmacParams()->hash().id());

  const std::vector<uint8_t> message_raw = HexStringToBytes(
      "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a"
      "92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92"
      "d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f"
      "22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e");

  std::vector<uint8_t> output;

  ASSERT_EQ(Status::Success(),
            Sign(CreateAlgorithm(blink::WebCryptoAlgorithmIdHmac),
                 key,
                 CryptoData(message_raw),
                 &output));

  const std::string mac_raw =
      "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b";

  EXPECT_BYTES_EQ_HEX(mac_raw, output);

  // TODO(padolph): Import an RSA public key JWK and use it
}

TEST(WebCryptoHmacTest, ImportExportJwk) {
  // HMAC SHA-1
  ImportExportJwkSymmetricKey(
      256,
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha1),
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      "HS1");

  // HMAC SHA-384
  ImportExportJwkSymmetricKey(
      384,
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha384),
      blink::WebCryptoKeyUsageSign,
      "HS384");

  // HMAC SHA-512
  ImportExportJwkSymmetricKey(
      512,
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha512),
      blink::WebCryptoKeyUsageVerify,
      "HS512");

  // Zero usage value
  ImportExportJwkSymmetricKey(
      512,
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha512),
      0,
      "HS512");
}

TEST(WebCryptoHmacTest, ExportJwkEmptyKey) {
  const blink::WebCryptoAlgorithm import_algorithm =
      CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha1);

  blink::WebCryptoKeyUsageMask usages = blink::WebCryptoKeyUsageSign;
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // Import a zero-byte HMAC key.
  const char key_data_hex[] = "";
  key = ImportSecretKeyFromRaw(
      HexStringToBytes(key_data_hex), import_algorithm, usages);
  EXPECT_EQ(0u, key.algorithm().hmacParams()->lengthBits());

  // Export the key in JWK format and validate.
  std::vector<uint8_t> json;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatJwk, key, &json));
  EXPECT_TRUE(VerifySecretJwk(json, "HS1", key_data_hex, usages));

  // Now try re-importing the JWK key.
  key = blink::WebCryptoKey::createNull();
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatJwk,
                      CryptoData(json),
                      import_algorithm,
                      true,
                      usages,
                      &key));

  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  EXPECT_EQ(0u, key.algorithm().hmacParams()->lengthBits());

  std::vector<uint8_t> exported_key_data;
  EXPECT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatRaw, key, &exported_key_data));

  EXPECT_EQ(0u, exported_key_data.size());
}

// Import a huge hmac key (UINT_MAX bytes). This will fail before actually
// reading the bytes, as the key is too large.
TEST(WebCryptoHmacTest, ImportRawKeyTooLarge) {
  CryptoData big_data(NULL, UINT_MAX);  // Invalid data of big length.

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  EXPECT_EQ(
      Status::ErrorDataTooLarge(),
      ImportKey(blink::WebCryptoKeyFormatRaw,
                CryptoData(big_data),
                CreateHmacImportAlgorithm(blink::WebCryptoAlgorithmIdSha1),
                true,
                blink::WebCryptoKeyUsageSign,
                &key));
}

}  // namespace

}  // namespace webcrypto

}  // namespace content
