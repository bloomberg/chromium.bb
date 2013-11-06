// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace {

std::vector<uint8> HexStringToBytes(const std::string& hex) {
  std::vector<uint8> bytes;
  base::HexStringToBytes(hex, &bytes);
  return bytes;
}

void ExpectArrayBufferMatchesHex(const std::string& expected_hex,
                                 const WebKit::WebArrayBuffer& array_buffer) {
  EXPECT_STRCASEEQ(
      expected_hex.c_str(),
      base::HexEncode(
          array_buffer.data(), array_buffer.byteLength()).c_str());
}

WebKit::WebCryptoAlgorithm CreateAlgorithm(WebKit::WebCryptoAlgorithmId id) {
  return WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

WebKit::WebCryptoAlgorithm CreateHmacAlgorithm(
    WebKit::WebCryptoAlgorithmId hashId) {
  return WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(
      WebKit::WebCryptoAlgorithmIdHmac,
      new WebKit::WebCryptoHmacParams(CreateAlgorithm(hashId)));
}

WebKit::WebCryptoAlgorithm CreateHmacKeyAlgorithm(
    WebKit::WebCryptoAlgorithmId hashId,
    unsigned hash_length) {
  // hash_length < 0 means unspecified
  return WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(
      WebKit::WebCryptoAlgorithmIdHmac,
      new WebKit::WebCryptoHmacKeyParams(CreateAlgorithm(hashId),
                                         (hash_length != 0),
                                         hash_length));
}

// Returns a pointer to the start of |data|, or NULL if it is empty. This is a
// convenience function for getting the pointer, and should not be used beyond
// the expected lifetime of |data|.
const uint8* Start(const std::vector<uint8>& data) {
  if (data.empty())
    return NULL;
  return &data[0];
}

WebKit::WebCryptoAlgorithm CreateAesCbcAlgorithm(
    const std::vector<uint8>& iv) {
  return WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(
      WebKit::WebCryptoAlgorithmIdAesCbc,
      new WebKit::WebCryptoAesCbcParams(Start(iv), iv.size()));
}

WebKit::WebCryptoAlgorithm CreateAesCbcAlgorithm(
    unsigned short key_length_bits) { // NOLINT
  return WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(
      WebKit::WebCryptoAlgorithmIdAesCbc,
      new WebKit::WebCryptoAesKeyGenParams(key_length_bits));
}

WebKit::WebCryptoAlgorithm CreateRsaAlgorithm(
    WebKit::WebCryptoAlgorithmId algorithm_id,
    unsigned modulus_length,
    const std::vector<uint8>& public_exponent) {
  DCHECK(algorithm_id == WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
         algorithm_id == WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         algorithm_id == WebKit::WebCryptoAlgorithmIdRsaOaep);
  return WebKit::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm_id,
      new WebKit::WebCryptoRsaKeyGenParams(
          modulus_length, Start(public_exponent), public_exponent.size()));
}

}  // namespace

namespace content {

class WebCryptoImplTest : public testing::Test {
 protected:
  WebKit::WebCryptoKey ImportSecretKeyFromRawHexString(
      const std::string& key_hex,
      const WebKit::WebCryptoAlgorithm& algorithm,
      WebKit::WebCryptoKeyUsageMask usage) {
    std::vector<uint8> key_raw = HexStringToBytes(key_hex);

    WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();
    bool extractable = true;
    EXPECT_TRUE(crypto_.ImportKeyInternal(WebKit::WebCryptoKeyFormatRaw,
                                          Start(key_raw),
                                          key_raw.size(),
                                          algorithm,
                                          extractable,
                                          usage,
                                          &key));

    EXPECT_EQ(WebKit::WebCryptoKeyTypeSecret, key.type());
    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    return key;
  }

  // Forwarding methods to gain access to protected methods of
  // WebCryptoImpl.

  bool DigestInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const std::vector<uint8>& data,
      WebKit::WebArrayBuffer* buffer) {
    return crypto_.DigestInternal(algorithm, Start(data), data.size(), buffer);
  }

  bool GenerateKeyInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      WebKit::WebCryptoKey* key) {
    bool extractable = true;
    WebKit::WebCryptoKeyUsageMask usage_mask = 0;
    return crypto_.GenerateKeyInternal(algorithm, extractable, usage_mask, key);
  }

  bool GenerateKeyPairInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      bool extractable,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoKey* public_key,
      WebKit::WebCryptoKey* private_key) {
    return crypto_.GenerateKeyPairInternal(
        algorithm, extractable, usage_mask, public_key, private_key);
  }

  bool ImportKeyInternal(
      WebKit::WebCryptoKeyFormat format,
      const std::vector<uint8>& key_data,
      const WebKit::WebCryptoAlgorithm& algorithm,
      WebKit::WebCryptoKeyUsageMask usage_mask,
      WebKit::WebCryptoKey* key) {
    bool extractable = true;
    return crypto_.ImportKeyInternal(format,
                                     Start(key_data),
                                     key_data.size(),
                                     algorithm,
                                     extractable,
                                     usage_mask,
                                     key);
  }

  bool SignInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const std::vector<uint8>& data,
      WebKit::WebArrayBuffer* buffer) {
    return crypto_.SignInternal(
        algorithm, key, Start(data), data.size(), buffer);
  }

  bool VerifySignatureInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned signature_size,
      const std::vector<uint8>& data,
      bool* signature_match) {
    return crypto_.VerifySignatureInternal(algorithm,
                                           key,
                                           signature,
                                           signature_size,
                                           Start(data),
                                           data.size(),
                                           signature_match);
  }

  bool EncryptInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(algorithm, key, data, data_size, buffer);
  }

  bool EncryptInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const std::vector<uint8>& data,
      WebKit::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(
        algorithm, key, Start(data), data.size(), buffer);
  }

  bool DecryptInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      WebKit::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(algorithm, key, data, data_size, buffer);
  }

  bool DecryptInternal(
      const WebKit::WebCryptoAlgorithm& algorithm,
      const WebKit::WebCryptoKey& key,
      const std::vector<uint8>& data,
      WebKit::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(
        algorithm, key, Start(data), data.size(), buffer);
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
    WebKit::WebCryptoAlgorithmId algorithm;
    const std::string hex_input;
    const char* hex_result;
  };

  const TestCase kTests[] = {
    { WebKit::WebCryptoAlgorithmIdSha1, "",
      "da39a3ee5e6b4b0d3255bfef95601890afd80709"
    },
    { WebKit::WebCryptoAlgorithmIdSha224, "",
      "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"
    },
    { WebKit::WebCryptoAlgorithmIdSha256, "",
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    },
    { WebKit::WebCryptoAlgorithmIdSha384, "",
      "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274e"
      "debfe76f65fbd51ad2f14898b95b"
    },
    { WebKit::WebCryptoAlgorithmIdSha512, "",
      "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0"
      "d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
    },
    { WebKit::WebCryptoAlgorithmIdSha1, "00",
      "5ba93c9db0cff93f52b521d7420e43f6eda2784f",
    },
    { WebKit::WebCryptoAlgorithmIdSha224, "00",
      "fff9292b4201617bdc4d3053fce02734166a683d7d858a7f5f59b073",
    },
    { WebKit::WebCryptoAlgorithmIdSha256, "00",
      "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d",
    },
    { WebKit::WebCryptoAlgorithmIdSha384, "00",
      "bec021b4f368e3069134e012c2b4307083d3a9bdd206e24e5f0d86e13d6636655933"
      "ec2b413465966817a9c208a11717",
    },
    { WebKit::WebCryptoAlgorithmIdSha512, "00",
      "b8244d028981d693af7b456af8efa4cad63d282e19ff14942c246e50d9351d22704a"
      "802a71c3580b6370de4ceb293c324a8423342557d4e5c38438f0e36910ee",
    },
    { WebKit::WebCryptoAlgorithmIdSha1, "000102030405",
      "868460d98d09d8bbb93d7b6cdd15cc7fbec676b9",
    },
    { WebKit::WebCryptoAlgorithmIdSha224, "000102030405",
      "7d92e7f1cad1818ed1d13ab41f04ebabfe1fef6bb4cbeebac34c29bc",
    },
    { WebKit::WebCryptoAlgorithmIdSha256, "000102030405",
      "17e88db187afd62c16e5debf3e6527cd006bc012bc90b51a810cd80c2d511f43",
    },
    { WebKit::WebCryptoAlgorithmIdSha384, "000102030405",
      "79f4738706fce9650ac60266675c3cd07298b09923850d525604d040e6e448adc7dc"
      "22780d7e1b95bfeaa86a678e4552",
    },
    { WebKit::WebCryptoAlgorithmIdSha512, "000102030405",
      "2f3831bccc94cf061bcfa5f8c23c1429d26e3bc6b76edad93d9025cb91c903af6cf9"
      "c935dc37193c04c2c66e7d9de17c358284418218afea2160147aaa912f4c",
    },
  };

  for (size_t test_index = 0; test_index < ARRAYSIZE_UNSAFE(kTests);
       ++test_index) {
    SCOPED_TRACE(test_index);
    const TestCase& test = kTests[test_index];

    WebKit::WebCryptoAlgorithm algorithm = CreateAlgorithm(test.algorithm);
    std::vector<uint8> input = HexStringToBytes(test.hex_input);

    WebKit::WebArrayBuffer output;
    ASSERT_TRUE(DigestInternal(algorithm, input, &output));
    ExpectArrayBufferMatchesHex(test.hex_result, output);
  }
}

TEST_F(WebCryptoImplTest, HMACSampleSets) {
  struct TestCase {
    WebKit::WebCryptoAlgorithmId algorithm;
    const char* key;
    const char* message;
    const char* mac;
  };

  const TestCase kTests[] = {
    // Empty sets.  Result generated via OpenSSL commandline tool.  These
    // particular results are also posted on the Wikipedia page examples:
    // http://en.wikipedia.org/wiki/Hash-based_message_authentication_code
    {
      WebKit::WebCryptoAlgorithmIdSha1,
      "",
      "",
      // openssl dgst -sha1 -hmac "" < /dev/null
      "fbdb1d1b18aa6c08324b7d64b71fb76370690e1d",
    },
    {
      WebKit::WebCryptoAlgorithmIdSha256,
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
      WebKit::WebCryptoAlgorithmIdSha1,
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
      WebKit::WebCryptoAlgorithmIdSha1,
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
      WebKit::WebCryptoAlgorithmIdSha256,
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
      WebKit::WebCryptoAlgorithmIdSha256,
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

    WebKit::WebCryptoAlgorithm algorithm = CreateHmacAlgorithm(test.algorithm);

    WebKit::WebCryptoKey key = ImportSecretKeyFromRawHexString(
        test.key, algorithm, WebKit::WebCryptoKeyUsageSign);

    std::vector<uint8> message_raw = HexStringToBytes(test.message);

    WebKit::WebArrayBuffer output;

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

TEST_F(WebCryptoImplTest, AesCbcFailures) {
  WebKit::WebCryptoKey key = ImportSecretKeyFromRawHexString(
      "2b7e151628aed2a6abf7158809cf4f3c",
      CreateAlgorithm(WebKit::WebCryptoAlgorithmIdAesCbc),
      WebKit::WebCryptoKeyUsageEncrypt | WebKit::WebCryptoKeyUsageDecrypt);

  WebKit::WebArrayBuffer output;

  // Use an invalid |iv| (fewer than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv;
    EXPECT_FALSE(
        EncryptInternal(CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_FALSE(
        DecryptInternal(CreateAesCbcAlgorithm(iv), key, input, &output));
  }

  // Use an invalid |iv| (more than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv(17);
    EXPECT_FALSE(
        EncryptInternal(CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_FALSE(
        DecryptInternal(CreateAesCbcAlgorithm(iv), key, input, &output));
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
        CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
    EXPECT_FALSE(DecryptInternal(
        CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
  }

  // Fail importing the key (too few bytes specified)
  {
    std::vector<uint8> key_raw(1);
    std::vector<uint8> iv(16);

    WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();
    EXPECT_FALSE(ImportKeyInternal(WebKit::WebCryptoKeyFormatRaw,
                                   key_raw,
                                   CreateAesCbcAlgorithm(iv),
                                   WebKit::WebCryptoKeyUsageDecrypt,
                                   &key));
  }
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

    WebKit::WebCryptoKey key = ImportSecretKeyFromRawHexString(
        test.key,
        CreateAlgorithm(WebKit::WebCryptoAlgorithmIdAesCbc),
        WebKit::WebCryptoKeyUsageEncrypt | WebKit::WebCryptoKeyUsageDecrypt);

    std::vector<uint8> plain_text = HexStringToBytes(test.plain_text);
    std::vector<uint8> iv = HexStringToBytes(test.iv);

    WebKit::WebArrayBuffer output;

    // Test encryption.
    EXPECT_TRUE(EncryptInternal(CreateAesCbcAlgorithm(iv),
                                key,
                                plain_text,
                                &output));
    ExpectArrayBufferMatchesHex(test.cipher_text, output);

    // Test decryption.
    std::vector<uint8> cipher_text = HexStringToBytes(test.cipher_text);
    EXPECT_TRUE(DecryptInternal(CreateAesCbcAlgorithm(iv),
                                key,
                                cipher_text,
                                &output));
    ExpectArrayBufferMatchesHex(test.plain_text, output);

    const unsigned kAesCbcBlockSize = 16;

    // Decrypt with a padding error by stripping the last block. This also ends
    // up testing decryption over empty cipher text.
    if (cipher_text.size() >= kAesCbcBlockSize) {
      EXPECT_FALSE(DecryptInternal(CreateAesCbcAlgorithm(iv),
                                   key,
                                   &cipher_text[0],
                                   cipher_text.size() - kAesCbcBlockSize,
                                   &output));
    }

    // Decrypt cipher text which is not a multiple of block size by stripping
    // a few bytes off the cipher text.
    if (cipher_text.size() > 3) {
      EXPECT_FALSE(DecryptInternal(CreateAesCbcAlgorithm(iv),
                                   key,
                                   &cipher_text[0],
                                   cipher_text.size() - 3,
                                   &output));
    }
  }
}

// TODO(padolph): Add test to verify generated symmetric keys appear random.

TEST_F(WebCryptoImplTest, GenerateKeyAes) {
  WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();
  ASSERT_TRUE(GenerateKeyInternal(CreateAesCbcAlgorithm(128), &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(WebKit::WebCryptoKeyTypeSecret, key.type());
}

TEST_F(WebCryptoImplTest, GenerateKeyAesBadLength) {
  WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();
  EXPECT_FALSE(GenerateKeyInternal(CreateAesCbcAlgorithm(0), &key));
  EXPECT_FALSE(GenerateKeyInternal(CreateAesCbcAlgorithm(129), &key));
}

TEST_F(WebCryptoImplTest, GenerateKeyHmac) {
  WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();
  WebKit::WebCryptoAlgorithm algorithm =
      CreateHmacKeyAlgorithm(WebKit::WebCryptoAlgorithmIdSha1, 128);
  ASSERT_TRUE(GenerateKeyInternal(algorithm, &key));
  EXPECT_FALSE(key.isNull());
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(WebKit::WebCryptoKeyTypeSecret, key.type());
}

TEST_F(WebCryptoImplTest, GenerateKeyHmacNoLength) {
  WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();
  WebKit::WebCryptoAlgorithm algorithm =
      CreateHmacKeyAlgorithm(WebKit::WebCryptoAlgorithmIdSha1, 0);
  ASSERT_TRUE(GenerateKeyInternal(algorithm, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(WebKit::WebCryptoKeyTypeSecret, key.type());
}

TEST_F(WebCryptoImplTest, ImportSecretKeyNoAlgorithm) {
  WebKit::WebCryptoKey key = WebKit::WebCryptoKey::createNull();

  // This fails because the algorithm is null.
  EXPECT_FALSE(ImportKeyInternal(
      WebKit::WebCryptoKeyFormatRaw,
      HexStringToBytes("00000000000000000000"),
      WebKit::WebCryptoAlgorithm::createNull(),
      WebKit::WebCryptoKeyUsageSign,
      &key));
}

#if !defined(USE_OPENSSL)

TEST_F(WebCryptoImplTest, GenerateKeyPairRsa) {
  // Note: using unrealistic short key lengths here to avoid bogging down tests.

  // Successful WebCryptoAlgorithmIdRsaEsPkcs1v1_5 key generation.
  const unsigned modulus_length = 256;
  const std::vector<uint8> public_exponent = HexStringToBytes("010001");
  WebKit::WebCryptoAlgorithm algorithm =
      CreateRsaAlgorithm(WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                         modulus_length,
                         public_exponent);
  bool extractable = false;
  const WebKit::WebCryptoKeyUsageMask usage_mask = 0;
  WebKit::WebCryptoKey public_key = WebKit::WebCryptoKey::createNull();
  WebKit::WebCryptoKey private_key = WebKit::WebCryptoKey::createNull();
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(extractable, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Fail with bad modulus.
  algorithm = CreateRsaAlgorithm(
      WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, 0, public_exponent);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: larger than unsigned long.
  unsigned exponent_length = sizeof(unsigned long) + 1; // NOLINT
  const std::vector<uint8> long_exponent(exponent_length, 0x01);
  algorithm = CreateRsaAlgorithm(WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                                 modulus_length,
                                 long_exponent);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: empty.
  const std::vector<uint8> empty_exponent;
  algorithm = CreateRsaAlgorithm(WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                                 modulus_length,
                                 empty_exponent);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: all zeros.
  std::vector<uint8> exponent_with_leading_zeros(15, 0x00);
  algorithm = CreateRsaAlgorithm(WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                                 modulus_length,
                                 exponent_with_leading_zeros);
  EXPECT_FALSE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Key generation success using exponent with leading zeros.
  exponent_with_leading_zeros.insert(exponent_with_leading_zeros.end(),
                                     public_exponent.begin(),
                                     public_exponent.end());
  algorithm = CreateRsaAlgorithm(WebKit::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
                                 modulus_length,
                                 exponent_with_leading_zeros);
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(extractable, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaOaep key generation.
  algorithm = CreateRsaAlgorithm(
      WebKit::WebCryptoAlgorithmIdRsaOaep, modulus_length, public_exponent);
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(extractable, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation.
  algorithm = CreateRsaAlgorithm(WebKit::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                 modulus_length,
                                 public_exponent);
  EXPECT_TRUE(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(WebKit::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(extractable, public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());
}

#endif  // #if !defined(USE_OPENSSL)

}  // namespace content
