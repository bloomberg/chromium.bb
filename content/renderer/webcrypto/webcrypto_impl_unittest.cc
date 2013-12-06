// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace {

std::vector<uint8> HexStringToBytes(const std::string& hex) {
  std::vector<uint8> bytes;
  base::HexStringToBytes(hex, &bytes);
  return bytes;
}

void ExpectArrayBufferMatchesHex(const std::string& expected_hex,
                                 const blink::WebArrayBuffer& array_buffer) {
  EXPECT_STRCASEEQ(
      expected_hex.c_str(),
      base::HexEncode(array_buffer.data(), array_buffer.byteLength()).c_str());
}

std::vector<uint8> MakeJsonVector(const std::string& json_string) {
  return std::vector<uint8>(json_string.begin(), json_string.end());
}

std::vector<uint8> MakeJsonVector(const base::DictionaryValue& dict) {
  std::string json;
  base::JSONWriter::Write(&dict, &json);
  return MakeJsonVector(json);
}

// Helper for ImportJwkFailures and ImportJwkOctFailures. Restores the JWK JSON
// dictionary to a good state
void RestoreJwkOctDictionary(base::DictionaryValue* dict) {
  dict->Clear();
  dict->SetString("kty", "oct");
  dict->SetString("alg", "A128CBC");
  dict->SetString("use", "enc");
  dict->SetBoolean("extractable", false);
  dict->SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
}

#if !defined(USE_OPENSSL)

// Helper for ImportJwkRsaFailures. Restores the JWK JSON
// dictionary to a good state
void RestoreJwkRsaDictionary(base::DictionaryValue* dict) {
  dict->Clear();
  dict->SetString("kty", "RSA");
  dict->SetString("alg", "RSA1_5");
  dict->SetString("use", "enc");
  dict->SetBoolean("extractable", false);
  dict->SetString("n",
      "qLOyhK-OtQs4cDSoYPFGxJGfMYdjzWxVmMiuSBGh4KvEx-CwgtaTpef87Wdc9GaFEncsDLxk"
      "p0LGxjD1M8jMcvYq6DPEC_JYQumEu3i9v5fAEH1VvbZi9cTg-rmEXLUUjvc5LdOq_5OuHmtm"
      "e7PUJHYW1PW6ENTP0ibeiNOfFvs");
  dict->SetString("e", "AQAB");
}

blink::WebCryptoAlgorithm CreateRsaKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId algorithm_id,
    unsigned modulus_length,
    const std::vector<uint8>& public_exponent) {
  DCHECK(algorithm_id == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaOaep);
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm_id,
      new blink::WebCryptoRsaKeyGenParams(
          modulus_length,
          webcrypto::Uint8VectorStart(public_exponent),
          public_exponent.size()));
}

// Determines if two ArrayBuffers have identical content.
bool ArrayBuffersEqual(
    const blink::WebArrayBuffer& a,
    const blink::WebArrayBuffer& b) {
  return a.byteLength() == b.byteLength() &&
         memcmp(a.data(), b.data(), a.byteLength()) == 0;
}

// Given a vector of WebArrayBuffers, determines if there are any copies.
bool CopiesExist(std::vector<blink::WebArrayBuffer> bufs) {
  for (size_t i = 0; i < bufs.size(); ++i) {
    for (size_t j = i + 1; j < bufs.size(); ++j) {
      if (ArrayBuffersEqual(bufs[i], bufs[j]))
        return true;
    }
  }
  return false;
}

#endif  // #if !defined(USE_OPENSSL)

}  // namespace

class WebCryptoImplTest : public testing::Test {
 protected:
  blink::WebCryptoKey ImportSecretKeyFromRawHexString(
      const std::string& key_hex,
      const blink::WebCryptoAlgorithm& algorithm,
      blink::WebCryptoKeyUsageMask usage) {
    std::vector<uint8> key_raw = HexStringToBytes(key_hex);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    bool extractable = true;
    EXPECT_TRUE(crypto_.ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          webcrypto::Uint8VectorStart(key_raw),
                                          key_raw.size(),
                                          algorithm,
                                          extractable,
                                          usage,
                                          &key));

    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    return key;
  }

  // Forwarding methods to gain access to protected methods of
  // WebCryptoImpl.

  bool DigestInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DigestInternal(
        algorithm, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  bool GenerateKeyInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      blink::WebCryptoKey* key) {
    bool extractable = true;
    blink::WebCryptoKeyUsageMask usage_mask = 0;
    return crypto_.GenerateKeyInternal(algorithm, extractable, usage_mask, key);
  }

  bool GenerateKeyPairInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) {
    return crypto_.GenerateKeyPairInternal(
        algorithm, extractable, usage_mask, public_key, private_key);
  }

  bool ImportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const std::vector<uint8>& key_data,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key) {
    return crypto_.ImportKeyInternal(format,
                                     webcrypto::Uint8VectorStart(key_data),
                                     key_data.size(),
                                     algorithm,
                                     extractable,
                                     usage_mask,
                                     key);
  }

  bool ExportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const blink::WebCryptoKey& key,
      blink::WebArrayBuffer* buffer) {
    return crypto_.ExportKeyInternal(format, key, buffer);
  }

  bool SignInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.SignInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  bool VerifySignatureInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned signature_size,
      const std::vector<uint8>& data,
      bool* signature_match) {
    return crypto_.VerifySignatureInternal(algorithm,
                                           key,
                                           signature,
                                           signature_size,
                                           webcrypto::Uint8VectorStart(data),
                                           data.size(),
                                           signature_match);
  }

  bool EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      blink::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(algorithm, key, data, data_size, buffer);
  }

  bool EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  bool DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(algorithm, key, data, data_size, buffer);
  }

  bool DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  bool ImportKeyJwk(
      const std::vector<uint8>& key_data,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key) {
    return crypto_.ImportKeyJwk(webcrypto::Uint8VectorStart(key_data),
                                key_data.size(),
                                algorithm,
                                extractable,
                                usage_mask,
                                key);
  }

 private:
  WebCryptoImpl crypto_;
};

TEST_F(WebCryptoImplTest, DigestSampleSets) {
  // The results are stored here in hex format for readability.
  //
  // TODO(bryaneyler): Eventually, all these sample test sets should be replaced
  // with the sets here: http://csrc.nist.gov/groups/STM/cavp/index.html#03
  //
  // Results were generated using the command sha{1,224,256,384,512}sum.
  struct TestCase {
    blink::WebCryptoAlgorithmId algorithm;
    const std::string hex_input;
    const char* hex_result;
  };

  const TestCase kTests[] = {
    { blink::WebCryptoAlgorithmIdSha1, "",
      "da39a3ee5e6b4b0d3255bfef95601890afd80709"
    },
    { blink::WebCryptoAlgorithmIdSha224, "",
      "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"
    },
    { blink::WebCryptoAlgorithmIdSha256, "",
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    },
    { blink::WebCryptoAlgorithmIdSha384, "",
      "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274e"
      "debfe76f65fbd51ad2f14898b95b"
    },
    { blink::WebCryptoAlgorithmIdSha512, "",
      "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0"
      "d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
    },
    { blink::WebCryptoAlgorithmIdSha1, "00",
      "5ba93c9db0cff93f52b521d7420e43f6eda2784f",
    },
    { blink::WebCryptoAlgorithmIdSha224, "00",
      "fff9292b4201617bdc4d3053fce02734166a683d7d858a7f5f59b073",
    },
    { blink::WebCryptoAlgorithmIdSha256, "00",
      "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
    },
    { blink::WebCryptoAlgorithmIdSha384, "00",
      "bec021b4f368e3069134e012c2b4307083d3a9bdd206e24e5f0d86e13d6636655933"
      "ec2b413465966817a9c208a11717",
    },
    { blink::WebCryptoAlgorithmIdSha512, "00",
      "b8244d028981d693af7b456af8efa4cad63d282e19ff14942c246e50d9351d22704a"
      "802a71c3580b6370de4ceb293c324a8423342557d4e5c38438f0e36910ee",
    },
    { blink::WebCryptoAlgorithmIdSha1, "000102030405",
      "868460d98d09d8bbb93d7b6cdd15cc7fbec676b9",
    },
    { blink::WebCryptoAlgorithmIdSha224, "000102030405",
      "7d92e7f1cad1818ed1d13ab41f04ebabfe1fef6bb4cbeebac34c29bc",
    },
    { blink::WebCryptoAlgorithmIdSha256, "000102030405",
      "17e88db187afd62c16e5debf3e6527cd006bc012bc90b51a810cd80c2d511f43",
    },
    { blink::WebCryptoAlgorithmIdSha384, "000102030405",
      "79f4738706fce9650ac60266675c3cd07298b09923850d525604d040e6e448adc7dc"
      "22780d7e1b95bfeaa86a678e4552",
    },
    { blink::WebCryptoAlgorithmIdSha512, "000102030405",
      "2f3831bccc94cf061bcfa5f8c23c1429d26e3bc6b76edad93d9025cb91c903af6cf9"
      "c935dc37193c04c2c66e7d9de17c358284418218afea2160147aaa912f4c",
    },
  };

  for (size_t test_index = 0; test_index < ARRAYSIZE_UNSAFE(kTests);
       ++test_index) {
    SCOPED_TRACE(test_index);
    const TestCase& test = kTests[test_index];

    blink::WebCryptoAlgorithm algorithm =
        webcrypto::CreateAlgorithm(test.algorithm);
    std::vector<uint8> input = HexStringToBytes(test.hex_input);

    blink::WebArrayBuffer output;
    ASSERT_TRUE(DigestInternal(algorithm, input, &output));
    ExpectArrayBufferMatchesHex(test.hex_result, output);
  }
}

TEST_F(WebCryptoImplTest, HMACSampleSets) {
  struct TestCase {
    blink::WebCryptoAlgorithmId algorithm;
    const char* key;
    const char* message;
    const char* mac;
  };

  const TestCase kTests[] = {
    // Empty sets.  Result generated via OpenSSL commandline tool.  These
    // particular results are also posted on the Wikipedia page examples:
    // http://en.wikipedia.org/wiki/Hash-based_message_authentication_code
    {
      blink::WebCryptoAlgorithmIdSha1,
      "",
      "",
      // openssl dgst -sha1 -hmac "" < /dev/null
      "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d",
    },
    {
      blink::WebCryptoAlgorithmIdSha256,
      "",
      "",
      // openssl dgst -sha256 -hmac "" < /dev/null
      "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad",
    },
    // For this data, see http://csrc.nist.gov/groups/STM/cavp/index.html#07
    // Download:
    // http://csrc.nist.gov/groups/STM/cavp/documents/mac/hmactestvectors.zip
    // L=20 set 45
    {
      blink::WebCryptoAlgorithmIdSha1,
      // key
      "59785928d72516e31272",
      // message
      "a3ce8899df1022e8d2d539b47bf0e309c66f84095e21438ec355bf119ce5fdcb4e73a6"
      "19cdf36f25b369d8c38ff419997f0c59830108223606e31223483fd39edeaa4d3f0d21"
      "198862d239c9fd26074130ff6c86493f5227ab895c8f244bd42c7afce5d147a20a5907"
      "98c68e708e964902d124dadecdbda9dbd0051ed710e9bf",
      // mac
      "3c8162589aafaee024fc9a5ca50dd2336fe3eb28",
    },
    // L=20 set 299
    {
      blink::WebCryptoAlgorithmIdSha1,
      // key
      "ceb9aedf8d6efcf0ae52bea0fa99a9e26ae81bacea0cff4d5eecf201e3bca3c3577480"
      "621b818fd717ba99d6ff958ea3d59b2527b019c343bb199e648090225867d994607962"
      "f5866aa62930d75b58f6",
      // message
      "99958aa459604657c7bf6e4cdfcc8785f0abf06ffe636b5b64ecd931bd8a4563055924"
      "21fc28dbcccb8a82acea2be8e54161d7a78e0399a6067ebaca3f2510274dc9f92f2c8a"
      "e4265eec13d7d42e9f8612d7bc258f913ecb5a3a5c610339b49fb90e9037b02d684fc6"
      "0da835657cb24eab352750c8b463b1a8494660d36c3ab2",
      // mac
      "4ac41ab89f625c60125ed65ffa958c6b490ea670",
    },
    // L=32, set 30
    {
      blink::WebCryptoAlgorithmIdSha256,
      // key
      "9779d9120642797f1747025d5b22b7ac607cab08e1758f2f3a46c8be1e25c53b8c6a8f"
      "58ffefa176",
      // message
      "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a"
      "92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92"
      "d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f"
      "22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e",
      // mac
      "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b",
    },
    // L=32, set 224
    {
      blink::WebCryptoAlgorithmIdSha256,
      // key
      "4b7ab133efe99e02fc89a28409ee187d579e774f4cba6fc223e13504e3511bef8d4f63"
      "8b9aca55d4a43b8fbd64cf9d74dcc8c9e8d52034898c70264ea911a3fd70813fa73b08"
      "3371289b",
      // message
      "138efc832c64513d11b9873c6fd4d8a65dbf367092a826ddd587d141b401580b798c69"
      "025ad510cff05fcfbceb6cf0bb03201aaa32e423d5200925bddfadd418d8e30e18050e"
      "b4f0618eb9959d9f78c1157d4b3e02cd5961f138afd57459939917d9144c95d8e6a94c"
      "8f6d4eef3418c17b1ef0b46c2a7188305d9811dccb3d99",
      // mac
      "4f1ee7cb36c58803a8721d4ac8c4cf8cae5d8832392eed2a96dc59694252801b",
    },
  };

  for (size_t test_index = 0; test_index < ARRAYSIZE_UNSAFE(kTests);
       ++test_index) {
    SCOPED_TRACE(test_index);
    const TestCase& test = kTests[test_index];

    blink::WebCryptoAlgorithm algorithm =
        webcrypto::CreateHmacAlgorithmByHashId(test.algorithm);

    blink::WebCryptoKey key = ImportSecretKeyFromRawHexString(
        test.key, algorithm, blink::WebCryptoKeyUsageSign);

    // Verify exported raw key is identical to the imported data
    blink::WebArrayBuffer raw_key;
    EXPECT_TRUE(ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatchesHex(test.key, raw_key);

    std::vector<uint8> message_raw = HexStringToBytes(test.message);

    blink::WebArrayBuffer output;

    ASSERT_TRUE(SignInternal(algorithm, key, message_raw, &output));

    ExpectArrayBufferMatchesHex(test.mac, output);

    bool signature_match = false;
    EXPECT_TRUE(VerifySignatureInternal(
        algorithm,
        key,
        static_cast<const unsigned char*>(output.data()),
        output.byteLength(),
        message_raw,
        &signature_match));
    EXPECT_TRUE(signature_match);

    // Ensure truncated signature does not verify by passing one less byte.
    EXPECT_TRUE(VerifySignatureInternal(
        algorithm,
        key,
        static_cast<const unsigned char*>(output.data()),
        output.byteLength() - 1,
        message_raw,
        &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure extra long signature does not cause issues and fails.
    const unsigned char kLongSignature[1024] = { 0 };
    EXPECT_TRUE(VerifySignatureInternal(
        algorithm,
        key,
        kLongSignature,
        sizeof(kLongSignature),
        message_raw,
        &signature_match));
    EXPECT_FALSE(signature_match);
  }
}

#if !defined(USE_OPENSSL)

TEST_F(WebCryptoImplTest, AesCbcFailures) {
  const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
  blink::WebCryptoKey key = ImportSecretKeyFromRawHexString(
      key_hex,
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

  // Verify exported raw key is identical to the imported data
  blink::WebArrayBuffer raw_key;
  EXPECT_TRUE(ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  ExpectArrayBufferMatchesHex(key_hex, raw_key);

  blink::WebArrayBuffer output;

  // Use an invalid |iv| (fewer than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv;
    EXPECT_FALSE(EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_FALSE(DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
  }

  // Use an invalid |iv| (more than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv(17);
    EXPECT_FALSE(EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_FALSE(DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
  }

  // Give an input that is too large (would cause integer overflow when
  // narrowing to an int).
  {
    std::vector<uint8> iv(16);

    // Pretend the input is large. Don't pass data pointer as NULL in case that
    // is special cased; the implementation shouldn't actually dereference the
    // data.
    const unsigned char* input = &iv[0];
    unsigned input_len = INT_MAX - 3;

    EXPECT_FALSE(EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
    EXPECT_FALSE(DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
  }

  // Fail importing the key (too few bytes specified)
  {
    std::vector<uint8> key_raw(1);
    std::vector<uint8> iv(16);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    EXPECT_FALSE(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                   key_raw,
                                   webcrypto::CreateAesCbcAlgorithm(iv),
                                   true,
                                   blink::WebCryptoKeyUsageEncrypt,
                                   &key));
  }

  // Fail exporting the key in SPKI and PKCS#8 formats (not allowed for secret
  // keys).
  EXPECT_FALSE(ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
  EXPECT_FALSE(ExportKeyInternal(blink::WebCryptoKeyFormatPkcs8, key, &output));
}

TEST_F(WebCryptoImplTest, AesCbcSampleSets) {
  struct TestCase {
    const char* key;
    const char* iv;
    const char* plain_text;
    const char* cipher_text;
  };

  TestCase kTests[] = {
    // F.2.1 (CBC-AES128.Encrypt)
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
    {
      // key
      "2b7e151628aed2a6abf7158809cf4f3c",

      // iv
      "000102030405060708090a0b0c0d0e0f",

      // plain_text
      "6bc1bee22e409f96e93d7e117393172a"
      "ae2d8a571e03ac9c9eb76fac45af8e51"
      "30c81c46a35ce411e5fbc1191a0a52ef"
      "f69f2445df4f9b17ad2b417be66c3710",

      // cipher_text
      "7649abac8119b246cee98e9b12e9197d"
      "5086cb9b507219ee95db113a917678b2"
      "73bed6b8e3c1743b7116e69e22229516"
      "3ff1caa1681fac09120eca307586e1a7"
      // Padding block: encryption of {0x10, 0x10, ... 0x10}) (not given by the
      // NIST test vector)
      "8cb82807230e1321d3fae00d18cc2012"
    },

    // F.2.6 CBC-AES256.Decrypt [*]
    // http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
    //
    // [*] Truncated 3 bytes off the plain text, so block 4 differs from the
    // NIST vector.
    {
      // key
      "603deb1015ca71be2b73aef0857d7781"
      "1f352c073b6108d72d9810a30914dff4",

      // iv
      "000102030405060708090a0b0c0d0e0f",

      // plain_text
      "6bc1bee22e409f96e93d7e117393172a"
      "ae2d8a571e03ac9c9eb76fac45af8e51"
      "30c81c46a35ce411e5fbc1191a0a52ef"
      // Truncated this last block to make it more interesting.
      "f69f2445df4f9b17ad2b417be6",

      // cipher_text
      "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
      "9cfc4e967edb808d679f777bc6702c7d"
      "39f23369a9d9bacfa530e26304231461"
      // This block differs from source vector (due to truncation)
      "c9aaf02a6a54e9e242ccbf48c59daca6"
    },

    // Taken from encryptor_unittest.cc (EncryptorTest.EmptyEncrypt())
    {
      // key
      "3132383d5369787465656e4279746573",

      // iv
      "5377656574205369787465656e204956",

      // plain_text
      "",

      // cipher_text
      "8518b8878d34e7185e300d0fcc426396"
    },
  };

  for (size_t index = 0; index < ARRAYSIZE_UNSAFE(kTests); index++) {
    SCOPED_TRACE(index);
    const TestCase& test = kTests[index];

    blink::WebCryptoKey key = ImportSecretKeyFromRawHexString(
        test.key,
        webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    // Verify exported raw key is identical to the imported data
    blink::WebArrayBuffer raw_key;
    EXPECT_TRUE(ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatchesHex(test.key, raw_key);

    std::vector<uint8> plain_text = HexStringToBytes(test.plain_text);
    std::vector<uint8> iv = HexStringToBytes(test.iv);

    blink::WebArrayBuffer output;

    // Test encryption.
    EXPECT_TRUE(EncryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                                key,
                                plain_text,
                                &output));
    ExpectArrayBufferMatchesHex(test.cipher_text, output);

    // Test decryption.
    std::vector<uint8> cipher_text = HexStringToBytes(test.cipher_text);
    EXPECT_TRUE(DecryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                                key,
                                cipher_text,
                                &output));
    ExpectArrayBufferMatchesHex(test.plain_text, output);

    const unsigned kAesCbcBlockSize = 16;

    // Decrypt with a padding error by stripping the last block. This also ends
    // up testing decryption over empty cipher text.
    if (cipher_text.size() >= kAesCbcBlockSize) {
      EXPECT_FALSE(DecryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                                   key,
                                   &cipher_text[0],
                                   cipher_text.size() - kAesCbcBlockSize,
                                   &output));
    }

    // Decrypt cipher text which is not a multiple of block size by stripping
    // a few bytes off the cipher text.
    if (cipher_text.size() > 3) {
      EXPECT_FALSE(DecryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                                   key,
                                   &cipher_text[0],
                                   cipher_text.size() - 3,
                                   &output));
    }
  }
}

TEST_F(WebCryptoImplTest, GenerateKeyAes) {
  // Generate a small sample of AES keys.
  std::vector<blink::WebArrayBuffer> keys;
  blink::WebArrayBuffer key_bytes;
  for (int i = 0; i < 16; ++i) {
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    ASSERT_TRUE(
        GenerateKeyInternal(webcrypto::CreateAesCbcKeyGenAlgorithm(128), &key));
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    ASSERT_TRUE(
        ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &key_bytes));
    keys.push_back(key_bytes);
  }
  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(keys));
}

TEST_F(WebCryptoImplTest, GenerateKeyAesBadLength) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  EXPECT_FALSE(
      GenerateKeyInternal(webcrypto::CreateAesCbcKeyGenAlgorithm(0), &key));
  EXPECT_FALSE(
      GenerateKeyInternal(webcrypto::CreateAesCbcKeyGenAlgorithm(0), &key));
  EXPECT_FALSE(
      GenerateKeyInternal(webcrypto::CreateAesCbcKeyGenAlgorithm(129), &key));
}

TEST_F(WebCryptoImplTest, GenerateKeyHmac) {
  // Generate a small sample of HMAC keys.
  std::vector<blink::WebArrayBuffer> keys;
  for (int i = 0; i < 16; ++i) {
    blink::WebArrayBuffer key_bytes;
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    blink::WebCryptoAlgorithm algorithm = webcrypto::CreateHmacKeyGenAlgorithm(
        blink::WebCryptoAlgorithmIdSha1, 128);
    ASSERT_TRUE(GenerateKeyInternal(algorithm, &key));
    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  }
  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(keys));
}

TEST_F(WebCryptoImplTest, GenerateKeyHmacNoLength) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacKeyGenAlgorithm(blink::WebCryptoAlgorithmIdSha1, 0);
  ASSERT_TRUE(GenerateKeyInternal(algorithm, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
}

TEST_F(WebCryptoImplTest, ImportSecretKeyNoAlgorithm) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // This fails because the algorithm is null.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatRaw,
      HexStringToBytes("00000000000000000000"),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
}

#endif  //#if !defined(USE_OPENSSL)

TEST_F(WebCryptoImplTest, ImportJwkFailures) {

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;

  // Baseline pass: each test below breaks a single item, so we start with a
  // passing case to make sure each failure is caused by the isolated break.
  // Each breaking subtest below resets the dictionary to this passing case when
  // complete.
  base::DictionaryValue dict;
  RestoreJwkOctDictionary(&dict);
  EXPECT_TRUE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));

  // Fail on empty JSON.
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(""), algorithm, false, usage_mask, &key));

  // Fail on invalid JSON.
  const std::vector<uint8> bad_json_vec = MakeJsonVector(
      "{"
        "\"kty\"         : \"oct\","
        "\"alg\"         : \"HS256\","
        "\"use\"         : "
  );
  EXPECT_FALSE(ImportKeyJwk(bad_json_vec, algorithm, false, usage_mask, &key));

  // Fail on JWK alg present but unrecognized.
  dict.SetString("alg", "A127CBC");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on both JWK and input algorithm missing.
  dict.Remove("alg", NULL);
  EXPECT_FALSE(ImportKeyJwk(MakeJsonVector(dict),
                            blink::WebCryptoAlgorithm::createNull(),
                            false,
                            usage_mask,
                            &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid kty.
  dict.SetString("kty", "foo");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on missing kty.
  dict.Remove("kty", NULL);
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid use.
  dict.SetString("use", "foo");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);
}

TEST_F(WebCryptoImplTest, ImportJwkOctFailures) {

  base::DictionaryValue dict;
  RestoreJwkOctDictionary(&dict);
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // Baseline pass.
  EXPECT_TRUE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_EQ(algorithm.id(), key.algorithm().id());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());

  // The following are specific failure cases for when kty = "oct".

  // Fail on missing k.
  dict.Remove("k", NULL);
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on bad b64 encoding for k.
  dict.SetString("k", "Qk3f0DsytU8lfza2au #$% Htaw2xpop9GYyTuH0p5GghxTI=");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on empty k.
  dict.SetString("k", "");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on k actual length (120 bits) inconsistent with the embedded JWK alg
  // value (128) for an AES key.
  dict.SetString("k", "AVj42h0Y5aqGtE3yluKL");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);
}

#if !defined(USE_OPENSSL)

TEST_F(WebCryptoImplTest, ImportJwkRsaFailures) {

  base::DictionaryValue dict;
  RestoreJwkRsaDictionary(&dict);
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // An RSA public key JWK _must_ have an "n" (modulus) and an "e" (exponent)
  // entry, while an RSA private key must have those plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.

  // Baseline pass.
  EXPECT_TRUE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_EQ(algorithm.id(), key.algorithm().id());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());

  // The following are specific failure cases for when kty = "RSA".

  // Fail if either "n" or "e" is not present or malformed.
  const std::string kKtyParmName[] = {"n", "e"};
  for (size_t idx = 0; idx < ARRAYSIZE_UNSAFE(kKtyParmName); ++idx) {

    // Fail on missing parameter.
    dict.Remove(kKtyParmName[idx], NULL);
    EXPECT_FALSE(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on bad b64 parameter encoding.
    dict.SetString(kKtyParmName[idx], "Qk3f0DsytU8lfza2au #$% Htaw2xpop9yTuH0");
    EXPECT_FALSE(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on empty parameter.
    dict.SetString(kKtyParmName[idx], "");
    EXPECT_FALSE(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);
  }

  // Fail if "d" parameter is present, implying the JWK is a private key, which
  // is not supported.
  dict.SetString("d", "Qk3f0Dsyt");
  EXPECT_FALSE(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkRsaDictionary(&dict);
}

#endif  // #if !defined(USE_OPENSSL)

TEST_F(WebCryptoImplTest, ImportJwkInputConsistency) {
  // The Web Crypto spec says that if a JWK value is present, but is
  // inconsistent with the input value, the operation must fail.

  // Consistency rules when JWK value is not present: Inputs should be used.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageVerify;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  std::vector<uint8> json_vec = MakeJsonVector(dict);
  EXPECT_TRUE(ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  EXPECT_EQ(extractable, key.extractable());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            key.algorithm().hmacParams()->hash().id());
  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
  key = blink::WebCryptoKey::createNull();

  // Consistency rules when JWK value exists: Fail if inconsistency is found.

  // Pass: All input values are consistent with the JWK values.
  dict.Clear();
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  dict.SetBoolean("extractable", false);
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  json_vec = MakeJsonVector(dict);
  EXPECT_TRUE(ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));

  // Extractable cases:
  // 1. input=T, JWK=F ==> fail (inconsistent)
  // 4. input=F, JWK=F ==> pass, result extractable is F
  // 2. input=T, JWK=T ==> pass, result extractable is T
  // 3. input=F, JWK=T ==> pass, result extractable is F
  EXPECT_FALSE(ImportKeyJwk(json_vec, algorithm, true, usage_mask, &key));
  EXPECT_TRUE(ImportKeyJwk(json_vec, algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("extractable", true);
  EXPECT_TRUE(
      ImportKeyJwk(MakeJsonVector(dict), algorithm, true, usage_mask, &key));
  EXPECT_TRUE(key.extractable());
  EXPECT_TRUE(
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("extractable", true);  // restore previous value

  // Fail: Input algorithm (AES-CBC) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_FALSE(ImportKeyJwk(
      json_vec,
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      extractable,
      usage_mask,
      &key));

  // Fail: Input algorithm (HMAC SHA1) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_FALSE(ImportKeyJwk(
      json_vec,
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha1),
      extractable,
      usage_mask,
      &key));

  // Pass: JWK alg valid but input algorithm isNull: use JWK algorithm value.
  EXPECT_TRUE(ImportKeyJwk(json_vec,
                           blink::WebCryptoAlgorithm::createNull(),
                           extractable,
                           usage_mask,
                           &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());

  // Pass: JWK alg missing but input algorithm specified: use input value
  dict.Remove("alg", NULL);
  EXPECT_TRUE(ImportKeyJwk(
      MakeJsonVector(dict),
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256),
      extractable,
      usage_mask,
      &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());
  dict.SetString("alg", "HS256");

  // Fail: Input usage_mask (encrypt) is not a subset of the JWK value
  // (sign|verify)
  EXPECT_FALSE(ImportKeyJwk(
      json_vec, algorithm, extractable, blink::WebCryptoKeyUsageEncrypt, &key));

  // Fail: Input usage_mask (encrypt|sign|verify) is not a subset of the JWK
  // value (sign|verify)
  usage_mask = blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageSign |
               blink::WebCryptoKeyUsageVerify;
  EXPECT_FALSE(
      ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));
  usage_mask = blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;

  // TODO(padolph): kty vs alg consistency tests: Depending on the kty value,
  // only certain alg values are permitted. For example, when kty = "RSA" alg
  // must be of the RSA family, or when kty = "oct" alg must be symmetric
  // algorithm.
}

TEST_F(WebCryptoImplTest, ImportJwkHappy) {

  // This test verifies the happy path of JWK import, including the application
  // of the imported key material.

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageSign;

  // Import a symmetric key JWK and HMAC-SHA256 sign()
  // Uses the first SHA256 test vector from the HMAC sample set above.

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  dict.SetBoolean("extractable", false);
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  std::vector<uint8> json_vec = MakeJsonVector(dict);

  ASSERT_TRUE(ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));

  const std::vector<uint8> message_raw = HexStringToBytes(
      "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a"
      "92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92"
      "d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f"
      "22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e");

  blink::WebArrayBuffer output;

  ASSERT_TRUE(SignInternal(algorithm, key, message_raw, &output));

  const std::string mac_raw =
      "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b";

  ExpectArrayBufferMatchesHex(mac_raw, output);

  // TODO(padolph): Import an RSA public key JWK and use it
}

#if !defined(USE_OPENSSL)

TEST_F(WebCryptoImplTest, ImportExportSpki) {
  // openssl genrsa -out pair.pem 2048
  // openssl rsa -in pair.pem -out pubkey.der -outform DER -pubout
  // xxd -p pubkey.der
  const std::string hex_rsa_spki_der =
      "30820122300d06092a864886f70d01010105000382010f003082010a0282"
      "010100f19e40f94e3780858701577a571cca000cb9795db89ddf8e98ab0e"
      "5eecfa47516cb08dc591cae5ab7fa43d6db402e95991d4a2de52e7cd3a66"
      "4f58284be2eb4675d5a849a2582c585d2b3c6c225a8f2c53a0414d5dbd06"
      "172371cefdf953e9ec3000fc9ad000743023f74e82d12aa93917a2c9b832"
      "696085ee0711154cf98a6d098f44cee00ea3b7584236503a5483ba8b6792"
      "fee588d1a8f4a0618333c4cb3447d760b43d5a0d9ed6ef79763df670cd8b"
      "5eb869a20833f1e3e6d8b88240a5d4335c73fd20487f2a7d112af8692357"
      "6425e44a273e5ad2e93d6b50a28e65f9e133958e4f0c7d12e0adc90fedd4"
      "f6b6848e7b6900666642a08b520a6534a35d4f0203010001";

  // Passing case: Import a valid RSA key in SPKI format.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_TRUE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(hex_rsa_spki_der),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  // Failing case: Empty SPKI data
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      std::vector<uint8>(),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Import RSA key with NULL input algorithm. This is not
  // allowed because the SPKI ASN.1 format for RSA keys is not specific enough
  // to map to a Web Crypto algorithm.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(hex_rsa_spki_der),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Bad DER encoding.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes("618333c4cb"),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(hex_rsa_spki_der),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Passing case: Export a previously imported RSA public key in SPKI format
  // and compare to original data.
  blink::WebArrayBuffer output;
  ASSERT_TRUE(ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
  ExpectArrayBufferMatchesHex(hex_rsa_spki_der, output);

  // Failing case: Try to export a previously imported RSA public key in raw
  // format (not allowed for a public key).
  EXPECT_FALSE(ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &output));

  // Failing case: Try to export a non-extractable key
  ASSERT_TRUE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(hex_rsa_spki_der),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      false,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_FALSE(key.extractable());
  EXPECT_FALSE(ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
}

TEST_F(WebCryptoImplTest, ImportPkcs8) {

  // The following is a DER-encoded PKCS#8 representation of the RSA key from
  // Example 1 of NIST's "Test vectors for RSA PKCS#1 v1.5 Signature".
  // ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
  const std::string hex_rsa_pkcs8_der =
      "30820275020100300D06092A864886F70D01010105000482025F3082025B020100028181"
      "00A56E4A0E701017589A5187DC7EA841D156F2EC0E36AD52A44DFEB1E61F7AD991D8C510"
      "56FFEDB162B4C0F283A12A88A394DFF526AB7291CBB307CEABFCE0B1DFD5CD9508096D5B"
      "2B8B6DF5D671EF6377C0921CB23C270A70E2598E6FF89D19F105ACC2D3F0CB35F29280E1"
      "386B6F64C4EF22E1E1F20D0CE8CFFB2249BD9A2137020301000102818033A5042A90B27D"
      "4F5451CA9BBBD0B44771A101AF884340AEF9885F2A4BBE92E894A724AC3C568C8F97853A"
      "D07C0266C8C6A3CA0929F1E8F11231884429FC4D9AE55FEE896A10CE707C3ED7E734E447"
      "27A39574501A532683109C2ABACABA283C31B4BD2F53C3EE37E352CEE34F9E503BD80C06"
      "22AD79C6DCEE883547C6A3B325024100E7E8942720A877517273A356053EA2A1BC0C94AA"
      "72D55C6E86296B2DFC967948C0A72CBCCCA7EACB35706E09A1DF55A1535BD9B3CC34160B"
      "3B6DCD3EDA8E6443024100B69DCA1CF7D4D7EC81E75B90FCCA874ABCDE123FD2700180AA"
      "90479B6E48DE8D67ED24F9F19D85BA275874F542CD20DC723E6963364A1F9425452B269A"
      "6799FD024028FA13938655BE1F8A159CBACA5A72EA190C30089E19CD274A556F36C4F6E1"
      "9F554B34C077790427BBDD8DD3EDE2448328F385D81B30E8E43B2FFFA02786197902401A"
      "8B38F398FA712049898D7FB79EE0A77668791299CDFA09EFC0E507ACB21ED74301EF5BFD"
      "48BE455EAEB6E1678255827580A8E4E8E14151D1510A82A3F2E729024027156ABA4126D2"
      "4A81F3A528CBFB27F56886F840A9F6E86E17A44B94FE9319584B8E22FDDE1E5A2E3BD8AA"
      "5BA8D8584194EB2190ACF832B847F13A3D24A79F4D";

  // Passing case: Import a valid RSA key in PKCS#8 format.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_TRUE(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(hex_rsa_pkcs8_der),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageSign, key.usages());

  // Failing case: Empty PKCS#8 data
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      std::vector<uint8>(),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Import RSA key with NULL input algorithm. This is not
  // allowed because the PKCS#8 ASN.1 format for RSA keys is not specific enough
  // to map to a Web Crypto algorithm.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(hex_rsa_pkcs8_der),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Bad DER encoding.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes("618333c4cb"),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_FALSE(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(hex_rsa_pkcs8_der),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));
}

TEST_F(WebCryptoImplTest, GenerateKeyPairRsa) {
  // Note: using unrealistic short key lengths here to avoid bogging down tests.

  // Successful WebCryptoAlgorithmIdRsaEsPkcs1v1_5 key generation.
  const unsigned modulus_length = 256;
  const std::vector<uint8> public_exponent = HexStringToBytes("010001");
  blink::WebCryptoAlgorithm algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      public_exponent);
  bool extractable = false;
  const blink::WebCryptoKeyUsageMask usage_mask = 0;
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(true, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Fail with bad modulus.
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, 0, public_exponent);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: larger than unsigned long.
  unsigned exponent_length = sizeof(unsigned long) + 1;  // NOLINT
  const std::vector<uint8> long_exponent(exponent_length, 0x01);
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, modulus_length, long_exponent);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: empty.
  const std::vector<uint8> empty_exponent;
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      empty_exponent);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: all zeros.
  std::vector<uint8> exponent_with_leading_zeros(15, 0x00);
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      exponent_with_leading_zeros);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Key generation success using exponent with leading zeros.
  exponent_with_leading_zeros.insert(exponent_with_leading_zeros.end(),
                                     public_exponent.begin(),
                                     public_exponent.end());
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      exponent_with_leading_zeros);
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(true, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaOaep key generation.
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaOaep, modulus_length, public_exponent);
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(true, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation.
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      modulus_length,
      public_exponent);
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(true, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Fail SPKI export of private key. This is an ExportKey test, but do it here
  // since it is expensive to generate an RSA key pair and we already have a
  // private key here.
  blink::WebArrayBuffer output;
  EXPECT_FALSE(
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, private_key, &output));
}

TEST_F(WebCryptoImplTest, RsaEsRoundTrip) {
  // Note: using unrealistic short key length here to avoid bogging down tests.

  // Create a key pair.
  const unsigned kModulusLength = 256;
  blink::WebCryptoAlgorithm algorithm =
      CreateRsaKeyGenAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                               kModulusLength,
                               HexStringToBytes("010001"));
  const blink::WebCryptoKeyUsageMask usage_mask =
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt;
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, false, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());

  // Make a maximum-length data message. RSAES can operate on messages up to
  // length of k - 11 bytes, where k is the octet length of the RSA modulus.
  const unsigned kMaxMsgSizeBytes = kModulusLength / 8 - 11;
  // There are two hex chars for each byte.
  const unsigned kMsgHexSize = kMaxMsgSizeBytes * 2;
  char max_data_hex[kMsgHexSize+1];
  std::fill(&max_data_hex[0], &max_data_hex[0] + kMsgHexSize, 'a');
  max_data_hex[kMsgHexSize] = '\0';

  // Verify encrypt / decrypt round trip on a few messages. Note that RSA
  // encryption does not support empty input.
  algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  const char* const kTestDataHex[] = {
      "ff",
      "0102030405060708090a0b0c0d0e0f",
      max_data_hex
  };
  blink::WebArrayBuffer encrypted_data;
  blink::WebArrayBuffer decrypted_data;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestDataHex); ++i) {
    SCOPED_TRACE(i);
    ASSERT_TRUE(EncryptInternal(
        algorithm,
        public_key,
        HexStringToBytes(kTestDataHex[i]),
        &encrypted_data));
    EXPECT_EQ(kModulusLength/8, encrypted_data.byteLength());
    ASSERT_TRUE(DecryptInternal(
        algorithm,
        private_key,
        reinterpret_cast<const unsigned char*>(encrypted_data.data()),
        encrypted_data.byteLength(),
        &decrypted_data));
    ExpectArrayBufferMatchesHex(kTestDataHex[i], decrypted_data);
  }
}

TEST_F(WebCryptoImplTest, RsaEsKnownAnswer) {
  // Because the random data in PKCS1.5 padding makes the encryption output non-
  // deterministic, we cannot easily do a typical known-answer test for RSA
  // encryption / decryption. Instead we will take a known-good encrypted
  // message, decrypt it, re-encrypt it, then decrypt again, verifying that the
  // original known cleartext is the result.

  // The RSA public and private keys used for this test are produced by the
  // openssl command line:
  // % openssl genrsa -out pair.pem 1024
  // % openssl rsa -in pair.pem -out spki.der -outform DER -pubout
  // % openssl pkcs8 -topk8 -inform PEM -outform DER -in pair.pem -out
  //     pkcs8.der -nocrypt
  // % xxd -p spki.der
  // % xxd -p pkcs8.der
  const std::string rsa_spki_der_hex =
      "30819f300d06092a864886f70d010101050003818d0030818902818100a8"
      "d30894b93f376f7822229bfd2483e50da944c4ab803ca31979e0f47e70bf"
      "683c687c6b3e80f280a237cea3643fd1f7f10f7cc664dbc2ecd45be53e1c"
      "9b15a53c37dbdad846c0f8340c472abc7821e4aa7df185867bf38228ac3e"
      "cc1d97d3c8b57e21ea6ba57b2bc3814a436e910ee8ab64a0b7743a927e94"
      "4d3420401f7dd50203010001";
  const std::string rsa_pkcs8_der_hex =
      "30820276020100300d06092a864886f70d0101010500048202603082025c"
      "02010002818100a8d30894b93f376f7822229bfd2483e50da944c4ab803c"
      "a31979e0f47e70bf683c687c6b3e80f280a237cea3643fd1f7f10f7cc664"
      "dbc2ecd45be53e1c9b15a53c37dbdad846c0f8340c472abc7821e4aa7df1"
      "85867bf38228ac3ecc1d97d3c8b57e21ea6ba57b2bc3814a436e910ee8ab"
      "64a0b7743a927e944d3420401f7dd5020301000102818100896cdffb50a0"
      "691bd00ad9696933243a7c5861a64684e8d74b91aed0d76c28234da9303e"
      "8c6ea2f89b141a9d5ea9a4ddd3d8eb9503dcf05ba0b1fd76060b281e3ae4"
      "b9d497fb5519bdf1127db8ad412d6a722686c78df3e3002acca960c6b2a2"
      "42a83ace5410693c03ce3d74cb9c9a7bacc8e271812920d1f53fee9312ef"
      "4eb1024100d09c14418ce92af7cc62f7cdc79836d8c6e3d0d33e7229cc11"
      "d732cbac75aa4c56c92e409a3ccbe75d4ce63ac5adca33080690782c6371"
      "e3628134c3534ca603024100cf2d3206f6deea2f39b70351c51f85436200"
      "5aa8f643e49e22486736d536e040dc30a2b4f9be3ab212a88d1891280874"
      "b9a170cdeb22eaf61c27c4b082c7d1470240638411a5b3b307ec6e744802"
      "c2d4ba556f8bfe72c7b76e790b89bd91ac13f5c9b51d04138d80b3450c1d"
      "4337865601bf96748b36c8f627be719f71ac3c70b441024065ce92cfe34e"
      "a58bf173a2b8f3024b4d5282540ac581957db3e11a7f528535ec098808dc"
      "a0013ffcb3b88a25716757c86c540e07d2ad8502cdd129118822c30f0240"
      "420a4983040e9db46eb29f1315a0d7b41cf60428f7460fce748e9a1a7d22"
      "d7390fa328948e7e9d1724401374e99d45eb41474781201378a4330e8e80"
      "8ce63551";

  // Similarly, the cleartext and public key encrypted ciphertext for this test
  // are also produced by openssl. Note that since we are using a 1024-bit key,
  // the cleartext size must be less than or equal to 117 bytes (modulusLength /
  // 8 - 11).
  // % openssl rand -out cleartext.bin 64
  // % openssl rsautl -encrypt -inkey spki.der -keyform DER -pubin -in
  //     cleartext.bin -out ciphertext.bin
  // % xxd -p cleartext.bin
  // % xxd -p ciphertext.bin
  const std::string cleartext_hex =
      "ec358ed141c45d7e03d4c6338aebad718e8bcbbf8f8ee6f8d9f4b9ef06d8"
      "84739a398c6bcbc688418b2ff64761dc0ccd40e7d52bed03e06946d0957a"
      "eef9e822";
  const std::string ciphertext_hex =
      "6106441c2b7a4b1a16260ed1ae4fe6135247345dc8e674754bbda6588c6c"
      "0d95a3d4d26bb34cdbcbe327723e80343bd7a15cd4c91c3a44e6cb9c6cd6"
      "7ad2e8bf41523188d9b36dc364a838642dcbc2c25e85dfb2106ba47578ca"
      "3bbf8915055aea4fa7c3cbfdfbcc163f04c234fb6d847f39bab9612ecbee"
      "04626e945c3ccf42";

  // Import the public key.
  const blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  ASSERT_TRUE(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(rsa_spki_der_hex),
      algorithm,
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &public_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_TRUE(public_key.handle());

  // Import the private key.
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ASSERT_TRUE(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(rsa_pkcs8_der_hex),
      algorithm,
      true,
      blink::WebCryptoKeyUsageDecrypt,
      &private_key));
  EXPECT_FALSE(private_key.isNull());
  EXPECT_TRUE(private_key.handle());

  // Decrypt the known-good ciphertext with the private key. As a check we must
  // get the known original cleartext.
  blink::WebArrayBuffer decrypted_data;
  ASSERT_TRUE(DecryptInternal(
      algorithm,
      private_key,
      HexStringToBytes(ciphertext_hex),
      &decrypted_data));
  EXPECT_FALSE(decrypted_data.isNull());
  ExpectArrayBufferMatchesHex(cleartext_hex, decrypted_data);

  // Encrypt this decrypted data with the public key.
  blink::WebArrayBuffer encrypted_data;
  ASSERT_TRUE(EncryptInternal(
      algorithm,
      public_key,
      reinterpret_cast<const unsigned char*>(decrypted_data.data()),
      decrypted_data.byteLength(),
      &encrypted_data));
  EXPECT_EQ(128u, encrypted_data.byteLength());

  // Finally, decrypt the newly encrypted result with the private key, and
  // compare to the known original cleartext.
  decrypted_data.reset();
  ASSERT_TRUE(DecryptInternal(
      algorithm,
      private_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));
  EXPECT_FALSE(decrypted_data.isNull());
  ExpectArrayBufferMatchesHex(cleartext_hex, decrypted_data);
}

TEST_F(WebCryptoImplTest, RsaEsFailures) {
  // Note: using unrealistic short key length here to avoid bogging down tests.

  // Create a key pair.
  const unsigned kModulusLength = 256;
  blink::WebCryptoAlgorithm algorithm =
      CreateRsaKeyGenAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                               kModulusLength,
                               HexStringToBytes("010001"));
  const blink::WebCryptoKeyUsageMask usage_mask =
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt;
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, false, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());

  // Fail encrypt with a private key.
  algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebArrayBuffer encrypted_data;
  const std::string message_hex_str("0102030405060708090a0b0c0d0e0f");
  const std::vector<uint8> message_hex(HexStringToBytes(message_hex_str));
  EXPECT_FALSE(
      EncryptInternal(algorithm, private_key, message_hex, &encrypted_data));

  // Fail encrypt with empty message.
  EXPECT_FALSE(EncryptInternal(
      algorithm, public_key, std::vector<uint8>(), &encrypted_data));

  // Fail encrypt with message too large. RSAES can operate on messages up to
  // length of k - 11 bytes, where k is the octet length of the RSA modulus.
  const unsigned kMaxMsgSizeBytes = kModulusLength / 8 - 11;
  EXPECT_FALSE(EncryptInternal(algorithm,
                               public_key,
                               std::vector<uint8>(kMaxMsgSizeBytes + 1, '0'),
                               &encrypted_data));

  // Generate encrypted data.
  EXPECT_TRUE(
      EncryptInternal(algorithm, public_key, message_hex, &encrypted_data));

  // Fail decrypt with a public key.
  blink::WebArrayBuffer decrypted_data;
  EXPECT_FALSE(DecryptInternal(
      algorithm,
      public_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));

  // Corrupt encrypted data; ensure decrypt fails because padding was disrupted.
  std::vector<uint8> corrupted_data(
      static_cast<uint8*>(encrypted_data.data()),
      static_cast<uint8*>(encrypted_data.data()) + encrypted_data.byteLength());
  corrupted_data[corrupted_data.size() / 2] ^= 0x01;
  EXPECT_FALSE(
      DecryptInternal(algorithm, private_key, corrupted_data, &decrypted_data));

  // TODO(padolph): Are there other specific data corruption scenarios to
  // consider?

  // Do a successful decrypt with good data just for confirmation.
  EXPECT_TRUE(DecryptInternal(
      algorithm,
      private_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));
  ExpectArrayBufferMatchesHex(message_hex_str, decrypted_data);
}

#endif  // #if !defined(USE_OPENSSL)

}  // namespace content
