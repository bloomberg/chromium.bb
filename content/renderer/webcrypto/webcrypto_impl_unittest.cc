// Copyright 2014 The Chromium Authors. All rights reserved.
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

// The OpenSSL implementation of WebCrypto is less complete, so don't run all of
// the tests: http://crbug.com/267888
#if defined(USE_OPENSSL)
#define MAYBE(test_name) DISABLED_##test_name
#else
#define MAYBE(test_name) test_name
#endif

// Helper macros to verify the value of a Status.
#define EXPECT_STATUS_ERROR(code) EXPECT_FALSE((code).IsSuccess())
#define EXPECT_STATUS(expected, code) \
    EXPECT_EQ(expected.ToString(), (code).ToString())
#define ASSERT_STATUS(expected, code) \
    ASSERT_EQ(expected.ToString(), (code).ToString())
#define EXPECT_STATUS_SUCCESS(code) EXPECT_STATUS(Status::Success(), code)
#define ASSERT_STATUS_SUCCESS(code) ASSERT_STATUS(Status::Success(), code)

namespace content {

using webcrypto::Status;

namespace {

// Returns a slightly modified version of the input vector.
//
//  - For non-empty inputs a single bit is inverted.
//  - For empty inputs, a byte is added.
std::vector<uint8> Corrupted(const std::vector<uint8>& input) {
  std::vector<uint8> corrupted_data(input);
  if (corrupted_data.empty())
    corrupted_data.push_back(0);
  corrupted_data[corrupted_data.size() / 2] ^= 0x01;
  return corrupted_data;
}

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

void ExpectVectorMatchesHex(const std::string& expected_hex,
                            const std::vector<uint8>& bytes) {
  EXPECT_STRCASEEQ(
      expected_hex.c_str(),
      base::HexEncode(webcrypto::Uint8VectorStart(bytes),
          bytes.size()).c_str());
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

blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8>& iv,
    const std::vector<uint8>& additional_data,
    unsigned tag_length_bits) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesGcm,
      new blink::WebCryptoAesGcmParams(
          webcrypto::Uint8VectorStart(iv), iv.size(),
          true,
          webcrypto::Uint8VectorStart(additional_data),
          additional_data.size(),
          true, tag_length_bits));
}

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

blink::WebCryptoAlgorithm CreateRsaAlgorithmWithInnerHash(
    blink::WebCryptoAlgorithmId algorithm_id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(algorithm_id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaOaep);
  DCHECK(webcrypto::IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm_id,
      new blink::WebCryptoRsaSsaParams(webcrypto::CreateAlgorithm(hash_id)));
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

blink::WebCryptoAlgorithm CreateAesKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId aes_alg_id,
    unsigned short length) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      aes_alg_id, new blink::WebCryptoAesKeyGenParams(length));
}

blink::WebCryptoAlgorithm CreateAesCbcKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesCbc,
                                  key_length_bits);
}

blink::WebCryptoAlgorithm CreateAesGcmKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesGcm,
                                  key_length_bits);
}

blink::WebCryptoAlgorithm CreateAesKwKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesKw,
                                  key_length_bits);
}

// The following key pair is comprised of the SPKI (public key) and PKCS#8
// (private key) representations of the key pair provided in Example 1 of the
// NIST test vectors at
// ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
const unsigned kModulusLength = 1024;
const char* const kPublicKeySpkiDerHex =
    "30819f300d06092a864886f70d010101050003818d0030818902818100a5"
    "6e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad9"
    "91d8c51056ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfc"
    "e0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e"
    "6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cf"
    "fb2249bd9a21370203010001";
const char* const kPrivateKeyPkcs8DerHex =
    "30820275020100300d06092a864886f70d01010105000482025f3082025b"
    "02010002818100a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52"
    "a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283a12a88a394dff526ab"
    "7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921c"
    "b23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef"
    "22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d"
    "4f5451ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c"
    "568c8f97853ad07c0266c8c6a3ca0929f1e8f11231884429fc4d9ae55fee"
    "896a10ce707c3ed7e734e44727a39574501a532683109c2abacaba283c31"
    "b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6dcee883547c6a3b3"
    "25024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8629"
    "6b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b"
    "3b6dcd3eda8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde12"
    "3fd2700180aa90479b6e48de8d67ed24f9f19d85ba275874f542cd20dc72"
    "3e6963364a1f9425452b269a6799fd024028fa13938655be1f8a159cbaca"
    "5a72ea190c30089e19cd274a556f36c4f6e19f554b34c077790427bbdd8d"
    "d3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa71"
    "2049898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd"
    "48be455eaeb6e1678255827580a8e4e8e14151d1510a82a3f2e729024027"
    "156aba4126d24a81f3a528cbfb27f56886f840a9f6e86e17a44b94fe9319"
    "584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb2190acf832b847f13a3d24"
    "a79f4d";

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
    EXPECT_STATUS_SUCCESS(
        crypto_.ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                  webcrypto::Uint8VectorStart(key_raw),
                                  key_raw.size(),
                                  algorithm,
                                  extractable,
                                  usage,
                                  &key));

    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    EXPECT_EQ(algorithm.id(), key.algorithm().id());
    EXPECT_EQ(extractable, key.extractable());
    EXPECT_EQ(usage, key.usages());
    return key;
  }

  void ImportRsaKeyPair(
      const std::string& spki_der_hex,
      const std::string& pkcs8_der_hex,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) {
    EXPECT_STATUS_SUCCESS(ImportKeyInternal(
        blink::WebCryptoKeyFormatSpki,
        HexStringToBytes(spki_der_hex),
        algorithm,
        true,
        usage_mask,
        public_key));
    EXPECT_FALSE(public_key->isNull());
    EXPECT_TRUE(public_key->handle());
    EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key->type());
    EXPECT_EQ(algorithm.id(), public_key->algorithm().id());
    EXPECT_EQ(extractable, extractable);
    EXPECT_EQ(usage_mask, public_key->usages());

    EXPECT_STATUS_SUCCESS(ImportKeyInternal(
        blink::WebCryptoKeyFormatPkcs8,
        HexStringToBytes(pkcs8_der_hex),
        algorithm,
        extractable,
        usage_mask,
        private_key));
    EXPECT_FALSE(private_key->isNull());
    EXPECT_TRUE(private_key->handle());
    EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key->type());
    EXPECT_EQ(algorithm.id(), private_key->algorithm().id());
    EXPECT_EQ(extractable, extractable);
    EXPECT_EQ(usage_mask, private_key->usages());
  }

  // TODO(eroman): For Linux builds using system NSS, AES-GCM support is a
  // runtime dependency. Test it by trying to import a key.
  bool SupportsAesGcm() {
    std::vector<uint8> key_raw(16, 0);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    Status status = crypto_.ImportKeyInternal(
        blink::WebCryptoKeyFormatRaw,
        webcrypto::Uint8VectorStart(key_raw),
        key_raw.size(),
        webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesGcm),
        true,
        blink::WebCryptoKeyUsageEncrypt,
        &key);

    if (status.IsError()) {
      EXPECT_EQ(Status::ErrorUnsupported().ToString(), status.ToString());
    }
    return status.IsSuccess();

  }

  Status AesGcmEncrypt(const blink::WebCryptoKey& key,
                       const std::vector<uint8>& iv,
                       const std::vector<uint8>& additional_data,
                       unsigned tag_length_bits,
                       const std::vector<uint8>& plain_text,
                       std::vector<uint8>* cipher_text,
                       std::vector<uint8>* authentication_tag) {
    blink::WebCryptoAlgorithm algorithm = CreateAesGcmAlgorithm(
        iv, additional_data, tag_length_bits);

    blink::WebArrayBuffer output;
    Status status = EncryptInternal(algorithm, key, plain_text, &output);
    if (status.IsError()) {
      return status;
    }

    if (output.byteLength() * 8 < tag_length_bits) {
      EXPECT_TRUE(false);
      return Status::Error();
    }

    // The encryption result is cipher text with authentication tag appended.
    cipher_text->assign(
        static_cast<uint8*>(output.data()),
        static_cast<uint8*>(output.data()) +
            (output.byteLength() - tag_length_bits / 8));
    authentication_tag->assign(
        static_cast<uint8*>(output.data()) + cipher_text->size(),
        static_cast<uint8*>(output.data()) + output.byteLength());

    return Status::Success();
  }

  Status AesGcmDecrypt(const blink::WebCryptoKey& key,
                       const std::vector<uint8>& iv,
                       const std::vector<uint8>& additional_data,
                       unsigned tag_length_bits,
                       const std::vector<uint8>& cipher_text,
                       const std::vector<uint8>& authentication_tag,
                       blink::WebArrayBuffer* plain_text) {
    blink::WebCryptoAlgorithm algorithm = CreateAesGcmAlgorithm(
        iv, additional_data, tag_length_bits);

    // Join cipher text and authentication tag.
    std::vector<uint8> cipher_text_with_tag;
    cipher_text_with_tag.reserve(
        cipher_text.size() + authentication_tag.size());
    cipher_text_with_tag.insert(
        cipher_text_with_tag.end(), cipher_text.begin(), cipher_text.end());
    cipher_text_with_tag.insert(
        cipher_text_with_tag.end(), authentication_tag.begin(),
        authentication_tag.end());

    return DecryptInternal(algorithm, key, cipher_text_with_tag, plain_text);
  }

  // Forwarding methods to gain access to protected methods of
  // WebCryptoImpl.

  Status DigestInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DigestInternal(
        algorithm, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status GenerateKeyInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      blink::WebCryptoKey* key) {
    bool extractable = true;
    blink::WebCryptoKeyUsageMask usage_mask = 0;
    return crypto_.GenerateSecretKeyInternal(
        algorithm, extractable, usage_mask, key);
  }

  Status GenerateKeyPairInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) {
    return crypto_.GenerateKeyPairInternal(
        algorithm, extractable, usage_mask, public_key, private_key);
  }

  Status ImportKeyInternal(
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

  Status ExportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const blink::WebCryptoKey& key,
      blink::WebArrayBuffer* buffer) {
    return crypto_.ExportKeyInternal(format, key, buffer);
  }

  Status SignInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.SignInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status VerifySignatureInternal(
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

  Status VerifySignatureInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& signature,
      const std::vector<uint8>& data,
      bool* signature_match) {
    return crypto_.VerifySignatureInternal(
        algorithm,
        key,
        webcrypto::Uint8VectorStart(signature),
        signature.size(),
        webcrypto::Uint8VectorStart(data),
        data.size(),
        signature_match);
  }

  Status EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      blink::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(algorithm, key, data, data_size, buffer);
  }

  Status EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned data_size,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(algorithm, key, data, data_size, buffer);
  }

  Status DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status ImportKeyJwk(
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

TEST_F(WebCryptoImplTest, StatusToString) {
  EXPECT_EQ("Success", Status::Success().ToString());
  EXPECT_EQ("", Status::Error().ToString());
  EXPECT_EQ("The requested operation is unsupported",
            Status::ErrorUnsupported().ToString());
}

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
    ASSERT_STATUS_SUCCESS(DigestInternal(algorithm, input, &output));
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
    // L=28, Count=71
    {
      blink::WebCryptoAlgorithmIdSha224,
      // key
      "6c2539f4d0453efbbacc137794930413aeb392e029e0724715f9d943d6dcf7cdcc7fc19"
      "7333df4fc476d5737ac3940d40eae",
      // message
      "1f207b3fa6c905529c9f9f7894b8941b616974df2c0cc482c400f50734f293139b5bbf9"
      "7384adfafc56494ca0629ed0ca179daf03056e33295eb19ec8dcd4dff898281b4b9409c"
      "a369f662d49091a225a678b1ebb75818dcb6278a2d136319f78f9ba9df5031a4f6305ee"
      "fde5b761d2f196ee318e89bcc4acebc2e11ed3b5dc4",
      // mac
      "4a7d9d13705b0faba0db75356c8ee0635afff1544911c69c2fbb1ab2"
    },
    // L=48, Count=50
    {
      blink::WebCryptoAlgorithmIdSha384,
      // key
      "d137f3e6cc4af28554beb03ba7a97e60c9d3959cd3bb08068edbf68d402d0498c6ee0ae"
      "9e3a20dc7d8586e5c352f605cee19",
      // message
      "64a884670d1c1dff555483dcd3da305dfba54bdc4d817c33ccb8fe7eb2ebf6236241031"
      "09ec41644fa078491900c59a0f666f0356d9bc0b45bcc79e5fc9850f4543d96bc680090"
      "44add0838ac1260e80592fbc557b2ddaf5ed1b86d3ed8f09e622e567f1d39a340857f6a"
      "850cceef6060c48dac3dd0071fe68eb4ed2ed9aca01",
      // mac
      "c550fa53514da34f15e7f98ea87226ab6896cdfae25d3ec2335839f755cdc9a4992092e"
      "70b7e5bd422784380b6396cf5"
    },
    // L=64, Count=65
    {
      blink::WebCryptoAlgorithmIdSha512,
      // key
      "c367aeb5c02b727883ffe2a4ceebf911b01454beb328fb5d57fc7f11bf744576aba421e2"
      "a63426ea8109bd28ff21f53cd2bf1a11c6c989623d6ec27cdb0bbf458250857d819ff844"
      "08b4f3dce08b98b1587ee59683af8852a0a5f55bda3ab5e132b4010e",
      // message
      "1a7331c8ff1b748e3cee96952190fdbbe4ee2f79e5753bbb368255ee5b19c05a4ed9f1b2"
      "c72ff1e9b9cb0348205087befa501e7793770faf0606e9c901836a9bc8afa00d7db94ee2"
      "9eb191d5cf3fc3e8da95a0f9f4a2a7964289c3129b512bd890de8700a9205420f28a8965"
      "b6c67be28ba7fe278e5fcd16f0f22cf2b2eacbb9",
      // mac
      "4459066109cb11e6870fa9c6bfd251adfa304c0a2928ca915049704972edc560cc7c0bc3"
      "8249e9101aae2f7d4da62eaff83fb07134efc277de72b9e4ab360425"
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
    EXPECT_STATUS_SUCCESS(
        ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatchesHex(test.key, raw_key);

    std::vector<uint8> message_raw = HexStringToBytes(test.message);

    blink::WebArrayBuffer output;

    ASSERT_STATUS_SUCCESS(SignInternal(algorithm, key, message_raw, &output));

    ExpectArrayBufferMatchesHex(test.mac, output);

    bool signature_match = false;
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        static_cast<const unsigned char*>(output.data()),
        output.byteLength(),
        message_raw,
        &signature_match));
    EXPECT_TRUE(signature_match);

    // Ensure truncated signature does not verify by passing one less byte.
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        static_cast<const unsigned char*>(output.data()),
        output.byteLength() - 1,
        message_raw,
        &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure truncated signature does not verify by passing no bytes.
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        NULL,
        0,
        message_raw,
        &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure extra long signature does not cause issues and fails.
    const unsigned char kLongSignature[1024] = { 0 };
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
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
  const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
  blink::WebCryptoKey key = ImportSecretKeyFromRawHexString(
      key_hex,
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

  // Verify exported raw key is identical to the imported data
  blink::WebArrayBuffer raw_key;
  EXPECT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  ExpectArrayBufferMatchesHex(key_hex, raw_key);

  blink::WebArrayBuffer output;

  // Use an invalid |iv| (fewer than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv;
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
  }

  // Use an invalid |iv| (more than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv(17);
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), DecryptInternal(
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

    EXPECT_STATUS(Status::ErrorDataTooLarge(), EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
    EXPECT_STATUS(Status::ErrorDataTooLarge(), DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
  }

  // Fail importing the key (too few bytes specified)
  {
    std::vector<uint8> key_raw(1);
    std::vector<uint8> iv(16);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    EXPECT_STATUS(
        Status::Error(),
        ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                          key_raw,
                          webcrypto::CreateAesCbcAlgorithm(iv),
                          true,
                          blink::WebCryptoKeyUsageEncrypt,
                          &key));
  }

  // TODO(eroman): Enable for OpenSSL once implemented.
#if !defined(USE_OPENSSL)
  // Fail exporting the key in SPKI and PKCS#8 formats (not allowed for secret
  // keys).
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
  EXPECT_STATUS(Status::ErrorUnsupported(),
      ExportKeyInternal(blink::WebCryptoKeyFormatPkcs8, key, &output));
#endif
}

TEST_F(WebCryptoImplTest, MAYBE(AesCbcSampleSets)) {
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
    EXPECT_STATUS_SUCCESS(ExportKeyInternal(
        blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatchesHex(test.key, raw_key);

    std::vector<uint8> plain_text = HexStringToBytes(test.plain_text);
    std::vector<uint8> iv = HexStringToBytes(test.iv);

    blink::WebArrayBuffer output;

    // Test encryption.
    EXPECT_STATUS(
        Status::Success(),
        EncryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                        key,
                        plain_text,
                        &output));
    ExpectArrayBufferMatchesHex(test.cipher_text, output);

    // Test decryption.
    std::vector<uint8> cipher_text = HexStringToBytes(test.cipher_text);
    EXPECT_STATUS(
        Status::Success(),
        DecryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                        key,
                        cipher_text,
                        &output));
    ExpectArrayBufferMatchesHex(test.plain_text, output);

    const unsigned kAesCbcBlockSize = 16;

    // Decrypt with a padding error by stripping the last block. This also ends
    // up testing decryption over empty cipher text.
    if (cipher_text.size() >= kAesCbcBlockSize) {
      EXPECT_STATUS(
          Status::Error(),
          DecryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                          key,
                          &cipher_text[0],
                          cipher_text.size() - kAesCbcBlockSize,
                          &output));
    }

    // Decrypt cipher text which is not a multiple of block size by stripping
    // a few bytes off the cipher text.
    if (cipher_text.size() > 3) {
      EXPECT_STATUS(
          Status::Error(),
          DecryptInternal(webcrypto::CreateAesCbcAlgorithm(iv),
                          key,
                          &cipher_text[0],
                          cipher_text.size() - 3,
                          &output));
    }
  }
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyAes)) {
  // Check key generation for each of AES-CBC, AES-GCM, and AES-KW, and for each
  // allowed key length.
  std::vector<blink::WebCryptoAlgorithm> algorithm;
  const unsigned short kKeyLength[] = {128, 192, 256};
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kKeyLength); ++i) {
    algorithm.push_back(CreateAesCbcKeyGenAlgorithm(kKeyLength[i]));
    algorithm.push_back(CreateAesGcmKeyGenAlgorithm(kKeyLength[i]));
    algorithm.push_back(CreateAesKwKeyGenAlgorithm(kKeyLength[i]));
  }
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  std::vector<blink::WebArrayBuffer> keys;
  blink::WebArrayBuffer key_bytes;
  for (size_t i = 0; i < algorithm.size(); ++i) {
    SCOPED_TRACE(i);
    // Generate a small sample of keys.
    keys.clear();
    for (int j = 0; j < 16; ++j) {
      ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm[i], &key));
      EXPECT_TRUE(key.handle());
      EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
      ASSERT_STATUS_SUCCESS(
          ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &key_bytes));
      keys.push_back(key_bytes);
    }
    // Ensure all entries in the key sample set are unique. This is a simplistic
    // estimate of whether the generated keys appear random.
    EXPECT_FALSE(CopiesExist(keys));
  }
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyAesBadLength)) {
  const unsigned short kKeyLen[] = {0, 127, 257};
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kKeyLen); ++i) {
    SCOPED_TRACE(i);
    EXPECT_STATUS(Status::ErrorGenerateKeyLength(), GenerateKeyInternal(
        CreateAesCbcKeyGenAlgorithm(kKeyLen[i]), &key));
    EXPECT_STATUS(Status::ErrorGenerateKeyLength(), GenerateKeyInternal(
        CreateAesGcmKeyGenAlgorithm(kKeyLen[i]), &key));
    EXPECT_STATUS(Status::ErrorGenerateKeyLength(), GenerateKeyInternal(
        CreateAesKwKeyGenAlgorithm(kKeyLen[i]), &key));
  }
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyHmac)) {
  // Generate a small sample of HMAC keys.
  std::vector<blink::WebArrayBuffer> keys;
  for (int i = 0; i < 16; ++i) {
    blink::WebArrayBuffer key_bytes;
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    blink::WebCryptoAlgorithm algorithm = webcrypto::CreateHmacKeyGenAlgorithm(
        blink::WebCryptoAlgorithmIdSha1, 64);
    ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm, &key));
    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());

    blink::WebArrayBuffer raw_key;
    ASSERT_STATUS_SUCCESS(
        ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    EXPECT_EQ(64U, raw_key.byteLength());
    keys.push_back(raw_key);
  }
  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(keys));
}

// If the key length is not provided, then the block size is used.
TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyHmacNoLength)) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacKeyGenAlgorithm(blink::WebCryptoAlgorithmIdSha1, 0);
  ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  blink::WebArrayBuffer raw_key;
  ASSERT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_EQ(64U, raw_key.byteLength());

  // The block size for HMAC SHA-512 is larger.
  algorithm = webcrypto::CreateHmacKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdSha512, 0);
  ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm, &key));
  ASSERT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_EQ(128U, raw_key.byteLength());
}

TEST_F(WebCryptoImplTest, MAYBE(ImportSecretKeyNoAlgorithm)) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // This fails because the algorithm is null.
  EXPECT_STATUS(Status::ErrorMissingAlgorithmImportRawKey(), ImportKeyInternal(
      blink::WebCryptoKeyFormatRaw,
      HexStringToBytes("00000000000000000000"),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
}


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
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));

  // Fail on empty JSON.
  EXPECT_STATUS(Status::ErrorImportEmptyKeyData(), ImportKeyJwk(
      MakeJsonVector(""), algorithm, false, usage_mask, &key));

  // Fail on invalid JSON.
  const std::vector<uint8> bad_json_vec = MakeJsonVector(
      "{"
        "\"kty\"         : \"oct\","
        "\"alg\"         : \"HS256\","
        "\"use\"         : "
  );
  EXPECT_STATUS(Status::ErrorJwkNotDictionary(),
      ImportKeyJwk(bad_json_vec, algorithm, false, usage_mask, &key));

  // Fail on JWK alg present but unrecognized.
  dict.SetString("alg", "A127CBC");
  EXPECT_STATUS(Status::ErrorJwkUnrecognizedAlgorithm(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on both JWK and input algorithm missing.
  dict.Remove("alg", NULL);
  EXPECT_STATUS(
      Status::ErrorJwkAlgorithmMissing(),
      ImportKeyJwk(MakeJsonVector(dict),
                   blink::WebCryptoAlgorithm::createNull(),
                   false,
                   usage_mask,
                   &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid kty.
  dict.SetString("kty", "foo");
  EXPECT_STATUS(Status::ErrorJwkUnrecognizedKty(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on missing kty.
  dict.Remove("kty", NULL);
  EXPECT_STATUS(Status::ErrorJwkMissingKty(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid use.
  dict.SetString("use", "foo");
  EXPECT_STATUS(Status::ErrorJwkUnrecognizedUsage(), ImportKeyJwk(
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
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_EQ(algorithm.id(), key.algorithm().id());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());

  // The following are specific failure cases for when kty = "oct".

  // Fail on missing k.
  dict.Remove("k", NULL);
  EXPECT_STATUS(Status::ErrorJwkDecodeK(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on bad b64 encoding for k.
  dict.SetString("k", "Qk3f0DsytU8lfza2au #$% Htaw2xpop9GYyTuH0p5GghxTI=");
  EXPECT_STATUS(Status::ErrorJwkDecodeK(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on empty k.
  dict.SetString("k", "");
  EXPECT_STATUS(Status::ErrorJwkDecodeK(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on k actual length (120 bits) inconsistent with the embedded JWK alg
  // value (128) for an AES key.
  dict.SetString("k", "AVj42h0Y5aqGtE3yluKL");
  // TODO(eroman): This is failing for a different reason than the test
  //               expects.
  EXPECT_STATUS(Status::Error(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);
}

TEST_F(WebCryptoImplTest, MAYBE(ImportJwkRsaFailures)) {

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
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
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
    EXPECT_STATUS_ERROR(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on bad b64 parameter encoding.
    dict.SetString(kKtyParmName[idx], "Qk3f0DsytU8lfza2au #$% Htaw2xpop9yTuH0");
    EXPECT_STATUS_ERROR(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on empty parameter.
    dict.SetString(kKtyParmName[idx], "");
    EXPECT_STATUS_ERROR(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);
  }

  // Fail if "d" parameter is present, implying the JWK is a private key, which
  // is not supported.
  dict.SetString("d", "Qk3f0Dsyt");
  EXPECT_STATUS(Status::ErrorJwkRsaPrivateKeyUnsupported(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkRsaDictionary(&dict);
}

TEST_F(WebCryptoImplTest, MAYBE(ImportJwkInputConsistency)) {
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
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      json_vec, algorithm, extractable, usage_mask, &key));
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
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));

  // Extractable cases:
  // 1. input=T, JWK=F ==> fail (inconsistent)
  // 4. input=F, JWK=F ==> pass, result extractable is F
  // 2. input=T, JWK=T ==> pass, result extractable is T
  // 3. input=F, JWK=T ==> pass, result extractable is F
  EXPECT_STATUS(Status::ErrorJwkExtractableInconsistent(),
      ImportKeyJwk(json_vec, algorithm, true, usage_mask, &key));
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(json_vec, algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("extractable", true);
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(MakeJsonVector(dict), algorithm, true, usage_mask, &key));
  EXPECT_TRUE(key.extractable());
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("extractable", true);  // restore previous value

  // Fail: Input algorithm (AES-CBC) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_STATUS(Status::ErrorJwkAlgorithmInconsistent(), ImportKeyJwk(
      json_vec,
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      extractable,
      usage_mask,
      &key));

  // Fail: Input algorithm (HMAC SHA1) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_STATUS(Status::ErrorJwkAlgorithmInconsistent(), ImportKeyJwk(
      json_vec,
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha1),
      extractable,
      usage_mask,
      &key));

  // Pass: JWK alg valid but input algorithm isNull: use JWK algorithm value.
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(json_vec,
                        blink::WebCryptoAlgorithm::createNull(),
                        extractable,
                        usage_mask,
                        &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());

  // Pass: JWK alg missing but input algorithm specified: use input value
  dict.Remove("alg", NULL);
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict),
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256),
      extractable,
      usage_mask,
      &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());
  dict.SetString("alg", "HS256");

  // Fail: Input usage_mask (encrypt) is not a subset of the JWK value
  // (sign|verify)
  EXPECT_STATUS(Status::ErrorJwkUsageInconsistent(), ImportKeyJwk(
      json_vec, algorithm, extractable, blink::WebCryptoKeyUsageEncrypt, &key));

  // Fail: Input usage_mask (encrypt|sign|verify) is not a subset of the JWK
  // value (sign|verify)
  usage_mask = blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageSign |
               blink::WebCryptoKeyUsageVerify;
  EXPECT_STATUS(
      Status::ErrorJwkUsageInconsistent(),
      ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));

  // TODO(padolph): kty vs alg consistency tests: Depending on the kty value,
  // only certain alg values are permitted. For example, when kty = "RSA" alg
  // must be of the RSA family, or when kty = "oct" alg must be symmetric
  // algorithm.
}

TEST_F(WebCryptoImplTest, MAYBE(ImportJwkHappy)) {

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

  ASSERT_STATUS_SUCCESS(ImportKeyJwk(
      json_vec, algorithm, extractable, usage_mask, &key));

  const std::vector<uint8> message_raw = HexStringToBytes(
      "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a"
      "92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92"
      "d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f"
      "22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e");

  blink::WebArrayBuffer output;

  ASSERT_STATUS_SUCCESS(SignInternal(algorithm, key, message_raw, &output));

  const std::string mac_raw =
      "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b";

  ExpectArrayBufferMatchesHex(mac_raw, output);

  // TODO(padolph): Import an RSA public key JWK and use it
}

TEST_F(WebCryptoImplTest, MAYBE(ImportExportSpki)) {
  // Passing case: Import a valid RSA key in SPKI format.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  // Failing case: Empty SPKI data
  EXPECT_STATUS(Status::ErrorImportEmptyKeyData(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      std::vector<uint8>(),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Import RSA key with NULL input algorithm. This is not
  // allowed because the SPKI ASN.1 format for RSA keys is not specific enough
  // to map to a Web Crypto algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Bad DER encoding.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes("618333c4cb"),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Passing case: Export a previously imported RSA public key in SPKI format
  // and compare to original data.
  blink::WebArrayBuffer output;
  ASSERT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
  ExpectArrayBufferMatchesHex(kPublicKeySpkiDerHex, output);

  // Failing case: Try to export a previously imported RSA public key in raw
  // format (not allowed for a public key).
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &output));

  // Failing case: Try to export a non-extractable key
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      false,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_FALSE(key.extractable());
  EXPECT_STATUS(Status::ErrorKeyNotExtractable(),
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
}

TEST_F(WebCryptoImplTest, MAYBE(ImportPkcs8)) {
  // Passing case: Import a valid RSA key in PKCS#8 format.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageSign, key.usages());

  // Failing case: Empty PKCS#8 data
  EXPECT_STATUS(Status::ErrorImportEmptyKeyData(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      std::vector<uint8>(),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Import RSA key with NULL input algorithm. This is not
  // allowed because the PKCS#8 ASN.1 format for RSA keys is not specific enough
  // to map to a Web Crypto algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Bad DER encoding.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes("618333c4cb"),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyPairRsa)) {
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
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
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
  EXPECT_STATUS(Status::ErrorGenerateRsaZeroModulus(), GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: larger than unsigned long.
  unsigned exponent_length = sizeof(unsigned long) + 1;  // NOLINT
  const std::vector<uint8> long_exponent(exponent_length, 0x01);
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      long_exponent);
  EXPECT_STATUS(Status::ErrorGenerateKeyPublicExponent(),
      GenerateKeyPairInternal(algorithm, extractable, usage_mask, &public_key,
          &private_key));

  // Fail with bad exponent: empty.
  const std::vector<uint8> empty_exponent;
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      empty_exponent);
  EXPECT_STATUS(Status::ErrorGenerateKeyPublicExponent(),
      GenerateKeyPairInternal(algorithm, extractable, usage_mask, &public_key,
          &private_key));

  // Fail with bad exponent: all zeros.
  std::vector<uint8> exponent_with_leading_zeros(15, 0x00);
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      exponent_with_leading_zeros);
  EXPECT_STATUS(Status::ErrorGenerateKeyPublicExponent(),
      GenerateKeyPairInternal(algorithm, extractable, usage_mask, &public_key,
          &private_key));

  // Key generation success using exponent with leading zeros.
  exponent_with_leading_zeros.insert(exponent_with_leading_zeros.end(),
                                     public_exponent.begin(),
                                     public_exponent.end());
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      exponent_with_leading_zeros);
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
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
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
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
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
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
  // TODO(eroman): This test is failing for a different reason than expected by
  //               the test.
  EXPECT_STATUS(Status::ErrorKeyNotExtractable(), ExportKeyInternal(
      blink::WebCryptoKeyFormatSpki, private_key, &output));
}

TEST_F(WebCryptoImplTest, MAYBE(RsaEsRoundTrip)) {
  // Import a key pair.
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      kPublicKeySpkiDerHex,
      kPrivateKeyPkcs8DerHex,
      algorithm,
      false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      &public_key,
      &private_key);

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
    EXPECT_STATUS_SUCCESS(EncryptInternal(
        algorithm,
        public_key,
        HexStringToBytes(kTestDataHex[i]),
        &encrypted_data));
    EXPECT_EQ(kModulusLength / 8, encrypted_data.byteLength());
    ASSERT_STATUS_SUCCESS(DecryptInternal(
        algorithm,
        private_key,
        reinterpret_cast<const unsigned char*>(encrypted_data.data()),
        encrypted_data.byteLength(),
        &decrypted_data));
    ExpectArrayBufferMatchesHex(kTestDataHex[i], decrypted_data);
  }
}

TEST_F(WebCryptoImplTest, MAYBE(RsaEsKnownAnswer)) {
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

  // Import the key pair.
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      rsa_spki_der_hex,
      rsa_pkcs8_der_hex,
      algorithm,
      false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      &public_key,
      &private_key);

  // Decrypt the known-good ciphertext with the private key. As a check we must
  // get the known original cleartext.
  blink::WebArrayBuffer decrypted_data;
  ASSERT_STATUS_SUCCESS(DecryptInternal(
      algorithm,
      private_key,
      HexStringToBytes(ciphertext_hex),
      &decrypted_data));
  EXPECT_FALSE(decrypted_data.isNull());
  ExpectArrayBufferMatchesHex(cleartext_hex, decrypted_data);

  // Encrypt this decrypted data with the public key.
  blink::WebArrayBuffer encrypted_data;
  ASSERT_STATUS_SUCCESS(EncryptInternal(
      algorithm,
      public_key,
      reinterpret_cast<const unsigned char*>(decrypted_data.data()),
      decrypted_data.byteLength(),
      &encrypted_data));
  EXPECT_EQ(128u, encrypted_data.byteLength());

  // Finally, decrypt the newly encrypted result with the private key, and
  // compare to the known original cleartext.
  decrypted_data.reset();
  ASSERT_STATUS_SUCCESS(DecryptInternal(
      algorithm,
      private_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));
  EXPECT_FALSE(decrypted_data.isNull());
  ExpectArrayBufferMatchesHex(cleartext_hex, decrypted_data);
}

TEST_F(WebCryptoImplTest, MAYBE(RsaEsFailures)) {
  // Import a key pair.
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      kPublicKeySpkiDerHex,
      kPrivateKeyPkcs8DerHex,
      algorithm,
      false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      &public_key,
      &private_key);

  // Fail encrypt with a private key.
  blink::WebArrayBuffer encrypted_data;
  const std::string message_hex_str("0102030405060708090a0b0c0d0e0f");
  const std::vector<uint8> message_hex(HexStringToBytes(message_hex_str));
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      EncryptInternal(algorithm, private_key, message_hex, &encrypted_data));

  // Fail encrypt with empty message.
  EXPECT_STATUS(Status::Error(), EncryptInternal(
      algorithm, public_key, std::vector<uint8>(), &encrypted_data));

  // Fail encrypt with message too large. RSAES can operate on messages up to
  // length of k - 11 bytes, where k is the octet length of the RSA modulus.
  const unsigned kMaxMsgSizeBytes = kModulusLength / 8 - 11;
  EXPECT_STATUS(
      Status::ErrorDataTooLarge(),
      EncryptInternal(algorithm,
                      public_key,
                      std::vector<uint8>(kMaxMsgSizeBytes + 1, '0'),
                      &encrypted_data));

  // Generate encrypted data.
  EXPECT_STATUS(Status::Success(),
      EncryptInternal(algorithm, public_key, message_hex, &encrypted_data));

  // Fail decrypt with a public key.
  blink::WebArrayBuffer decrypted_data;
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(), DecryptInternal(
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
 EXPECT_STATUS(Status::Error(),
      DecryptInternal(algorithm, private_key, corrupted_data, &decrypted_data));

  // TODO(padolph): Are there other specific data corruption scenarios to
  // consider?

  // Do a successful decrypt with good data just for confirmation.
  EXPECT_STATUS_SUCCESS(DecryptInternal(
      algorithm,
      private_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));
  ExpectArrayBufferMatchesHex(message_hex_str, decrypted_data);
}

TEST_F(WebCryptoImplTest, MAYBE(RsaSsaSignVerifyFailures)) {
  // Import a key pair.
  blink::WebCryptoAlgorithm algorithm = CreateRsaAlgorithmWithInnerHash(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      kPublicKeySpkiDerHex,
      kPrivateKeyPkcs8DerHex,
      algorithm,
      false,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      &public_key,
      &private_key);

  blink::WebArrayBuffer signature;
  bool signature_match;

  // Compute a signature.
  const std::vector<uint8> data = HexStringToBytes("010203040506070809");
  ASSERT_STATUS_SUCCESS(SignInternal(algorithm, private_key, data, &signature));

  // Ensure truncated signature does not verify by passing one less byte.
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength() - 1,
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure truncated signature does not verify by passing no bytes.
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      NULL,
      0,
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure corrupted signature does not verify.
  std::vector<uint8> corrupt_sig(
      static_cast<uint8*>(signature.data()),
      static_cast<uint8*>(signature.data()) + signature.byteLength());
  corrupt_sig[corrupt_sig.size() / 2] ^= 0x1;
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      webcrypto::Uint8VectorStart(corrupt_sig),
      corrupt_sig.size(),
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure signatures that are greater than the modulus size fail.
  const unsigned long_message_size_bytes = 1024;
  DCHECK_GT(long_message_size_bytes, kModulusLength/8);
  const unsigned char kLongSignature[long_message_size_bytes] = { 0 };
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      kLongSignature,
      sizeof(kLongSignature),
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure that verifying using a private key, rather than a public key, fails.
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(), VerifySignatureInternal(
      algorithm,
      private_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength(),
      data,
      &signature_match));

  // Ensure that signing using a public key, rather than a private key, fails.
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      SignInternal(algorithm, public_key, data, &signature));

  // Ensure that signing and verifying with an incompatible algorithm fails.
  algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  EXPECT_STATUS(Status::ErrorUnsupported(),
      SignInternal(algorithm, private_key, data, &signature));
  EXPECT_STATUS(Status::ErrorUnsupported(), VerifySignatureInternal(
      algorithm,
      public_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength(),
      data,
      &signature_match));

  // Some crypto libraries (NSS) can automatically select the RSA SSA inner hash
  // based solely on the contents of the input signature data. In the Web Crypto
  // implementation, the inner hash should be specified uniquely by the input
  // algorithm parameter. To validate this behavior, call Verify with a computed
  // signature that used one hash type (SHA-1), but pass in an algorithm with a
  // different inner hash type (SHA-256). If the hash type is determined by the
  // signature itself (undesired), the verify will pass, while if the hash type
  // is specified by the input algorithm (desired), the verify will fail.

  // Compute a signature using SHA-1 as the inner hash.
  EXPECT_STATUS_SUCCESS(SignInternal(CreateRsaAlgorithmWithInnerHash(
                               blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                               blink::WebCryptoAlgorithmIdSha1),
                           private_key,
                           data,
                           &signature));

  // Now verify using an algorithm whose inner hash is SHA-256, not SHA-1. The
  // signature should not verify.
  // NOTE: public_key was produced by generateKey, and so its associated
  // algorithm has WebCryptoRsaKeyGenParams and not WebCryptoRsaSsaParams. Thus
  // it has no inner hash to conflict with the input algorithm.
  bool is_match;
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      CreateRsaAlgorithmWithInnerHash(
          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
          blink::WebCryptoAlgorithmIdSha256),
      public_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength(),
      data,
      &is_match));
  EXPECT_FALSE(is_match);
}

TEST_F(WebCryptoImplTest, MAYBE(RsaSignVerifyKnownAnswer)) {
  // Use the NIST test vectors from Example 1 of
  // ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
  // These vectors are known answers for RSA PKCS#1 v1.5 Signature with a SHA-1
  // digest, using a predefined key pair.

  struct TestCase {
    const std::string message_hex;
    const std::string signature_hex;
  };

  // The following data are the input messages and corresponding computed RSA
  // PKCS#1 v1.5 signatures from the NIST link above.
  const TestCase kTests[] = {
      // PKCS#1 v1.5 Signature Example 1.1
      {"cdc87da223d786df3b45e0bbbc721326d1ee2af806cc315475cc6f0d9c66e1b6"
       "2371d45ce2392e1ac92844c310102f156a0d8d52c1f4c40ba3aa65095786cb76"
       "9757a6563ba958fed0bcc984e8b517a3d5f515b23b8a41e74aa867693f90dfb0"
       "61a6e86dfaaee64472c00e5f20945729cbebe77f06ce78e08f4098fba41f9d61"
       "93c0317e8b60d4b6084acb42d29e3808a3bc372d85e331170fcbf7cc72d0b71c"
       "296648b3a4d10f416295d0807aa625cab2744fd9ea8fd223c42537029828bd16"
       "be02546f130fd2e33b936d2676e08aed1b73318b750a0167d0",
       "6bc3a06656842930a247e30d5864b4d819236ba7c68965862ad7dbc4e24af28e"
       "86bb531f03358be5fb74777c6086f850caef893f0d6fcc2d0c91ec013693b4ea"
       "00b80cd49aac4ecb5f8911afe539ada4a8f3823d1d13e472d1490547c659c761"
       "7f3d24087ddb6f2b72096167fc097cab18e9a458fcb634cdce8ee35894c484d7"},
      // PKCS#1 v1.5 Signature Example 1.2
      {"851384cdfe819c22ed6c4ccb30daeb5cf059bc8e1166b7e3530c4c233e2b5f8f"
       "71a1cca582d43ecc72b1bca16dfc7013226b9e",
       "84fd2ce734ec1da828d0f15bf49a8707c15d05948136de537a3db421384167c8"
       "6fae022587ee9e137daee754738262932d271c744c6d3a189ad4311bdb020492"
       "e322fbddc40406ea860d4e8ea2a4084aa98b9622a446756fdb740ddb3d91db76"
       "70e211661bbf8709b11c08a70771422d1a12def29f0688a192aebd89e0f896f8"},
      // PKCS#1 v1.5 Signature Example1.3
      {"a4b159941761c40c6a82f2b80d1b94f5aa2654fd17e12d588864679b54cd04ef"
       "8bd03012be8dc37f4b83af7963faff0dfa225477437c48017ff2be8191cf3955"
       "fc07356eab3f322f7f620e21d254e5db4324279fe067e0910e2e81ca2cab31c7"
       "45e67a54058eb50d993cdb9ed0b4d029c06d21a94ca661c3ce27fae1d6cb20f4"
       "564d66ce4767583d0e5f060215b59017be85ea848939127bd8c9c4d47b51056c"
       "031cf336f17c9980f3b8f5b9b6878e8b797aa43b882684333e17893fe9caa6aa"
       "299f7ed1a18ee2c54864b7b2b99b72618fb02574d139ef50f019c9eef4169713"
       "38e7d470",
       "0b1f2e5180e5c7b4b5e672929f664c4896e50c35134b6de4d5a934252a3a245f"
       "f48340920e1034b7d5a5b524eb0e1cf12befef49b27b732d2c19e1c43217d6e1"
       "417381111a1d36de6375cf455b3c9812639dbc27600c751994fb61799ecf7da6"
       "bcf51540afd0174db4033188556675b1d763360af46feeca5b60f882829ee7b2"},
      // PKCS#1 v1.5 Signature Example 1.4
      {"bc656747fa9eafb3f0",
       "45607ad611cf5747a41ac94d0ffec878bdaf63f6b57a4b088bf36e34e109f840"
       "f24b742ada16102dabf951cbc44f8982e94ed4cd09448d20ec0efa73545f80b6"
       "5406bed6194a61c340b4ad1568cbb75851049f11af1734964076e02029aee200"
       "e40e80be0f4361f69841c4f92a4450a2286d43289b405554c54d25c6ecb584f4"},
      // PKCS#1 v1.5 Signature Example 1.5
      {"b45581547e5427770c768e8b82b75564e0ea4e9c32594d6bff706544de0a8776"
       "c7a80b4576550eee1b2acabc7e8b7d3ef7bb5b03e462c11047eadd00629ae575"
       "480ac1470fe046f13a2bf5af17921dc4b0aa8b02bee6334911651d7f8525d10f"
       "32b51d33be520d3ddf5a709955a3dfe78283b9e0ab54046d150c177f037fdccc"
       "5be4ea5f68b5e5a38c9d7edcccc4975f455a6909b4",
       "54be9d90877515f450279c15b5f61ad6f15ecc95f18cbed82b65b1667a575809"
       "587994668044f3bc2ae7f884501f64f0b43f588cfa205a6ab704328c2d4ab92a"
       "7ae13440614d3e085f401da9ad28e2105e4a0edb681a6424df047388ce051ee9"
       "df7bc2163fe347520ad51ccd518064383e741acad3cbdc2cb5a7c68e868464c2"},
      // PKCS#1 v1.5 Signature Example 1.6
      {"10aae9a0ab0b595d0841207b700d48d75faedde3b775cd6b4cc88ae06e4694ec"
       "74ba18f8520d4f5ea69cbbe7cc2beba43efdc10215ac4eb32dc302a1f53dc6c4"
       "352267e7936cfebf7c8d67035784a3909fa859c7b7b59b8e39c5c2349f1886b7"
       "05a30267d402f7486ab4f58cad5d69adb17ab8cd0ce1caf5025af4ae24b1fb87"
       "94c6070cc09a51e2f9911311e3877d0044c71c57a993395008806b723ac38373"
       "d395481818528c1e7053739282053529510e935cd0fa77b8fa53cc2d474bd4fb"
       "3cc5c672d6ffdc90a00f9848712c4bcfe46c60573659b11e6457e861f0f604b6"
       "138d144f8ce4e2da73",
       "0e6ff63a856b9cbd5dbe423183122047dd39d6f76d1b2310e546fe9ee73b33ef"
       "a7c78f9474455c9e5b88cb383aafc3698668e7b7a59a9cbb5b0897b6c5afb7f8"
       "bac4b924e98d760a15fc43d2814ab2d5187f79bed9915a93397ebc22a7677506"
       "a02e076d3ffdc0441dbd4db00453dc28d830e0573f77b817b505c38b4a4bb5d0"},
      // PKCS#1 v1.5 Signature Example 1.7
      {"efb5da1b4d1e6d9a5dff92d0184da7e31f877d1281ddda625664869e8379e67a"
       "d3b75eae74a580e9827abd6eb7a002cb5411f5266797768fb8e95ae40e3e8b34"
       "66f5ab15d69553952939ec23e61d58497fac76aa1c0bb5a3cb4a54383587c7bb"
       "78d13eefda205443e6ce4365802df55c64713497984e7ca96722b3edf84d56",
       "8385d58533a995f72df262b70f40b391ddf515f464b9d2cc2d66398fc05689d8"
       "11632946d62eabdca7a31fcf6cd6c981d28bbc29083e4a6d5b2b378ca4e540f0"
       "60b96d53ad2693f82178b94e2e2f86b9accfa02025107e062ab7080175684501"
       "028f676461d81c008fe4750671649970878fc175cf98e96b2ecbf6874d77dacb"},
      // PKCS#1 v1.5 Signature Example 1.8
      {"53bb58ce42f1984940552657233b14969af365c0a561a4132af18af39432280e"
       "3e437082434b19231837184f02cf2b2e726bebf74d7ae3256d8b72f3eafdb134"
       "d33de06f2991d299d59f5468d43b9958d6a968f5969edbbc6e7185cbc716c7c9"
       "45dafa9cc71ddfaaa01094a452ddf5e2407320400bf05ea9729cafbf0600e788"
       "07ef9462e3fde32ed7d981a56f4751ef64fb4549910ecc911d728053b3994300"
       "4740e6f5821fe8d75c0617bf2c6b24bbfc34013fc95f0dedf5ba297f504fb833"
       "da2a436d1d8ff1cc5193e2a64389fced918e7feb6716330f66801db9497549cf"
       "1d3bd97cf1bc6255",
       "8e1f3d26ec7c6bbb8c54c5d25f3120587803af6d3c2b99a37ced6a3657d4ae54"
       "266f63fffde660c866d65d0ab0589e1d12d9ce6054b05c8668ae127171ccaae7"
       "f1cd409677f52157b6123ab227f27a00966d1439b42a32169d1070394026fc8b"
       "c93545b1ac252d0f7da751c02e33a47831fbd71514c2bbbd3adb6740c0fd68ad"},
      // PKCS#1 v1.5 Signature Example 1.9
      {"27cadc698450945f204ec3cf8c6cbd8ceb4cc0cbe312274fa96b04deac855160"
       "c0e04e4ac5d38210c27c",
       "7b63f9223356f35f6117f68c8f8220034fc2384ab5dc6904141f139314d6ee89"
       "f54ec6ffd18c413a23c5931c7fbb13c555ccfd590e0eaa853c8c94d2520cd425"
       "0d9a05a193b65dc749b82478af0156ee1de55ddad33ec1f0099cad6c891a3617"
       "c7393d05fbfbbb00528a001df0b204ebdf1a341090dea89f870a877458427f7b"},
      // PKCS#1 v1.5 Signature Example 1.10
      {"716407e901b9ef92d761b013fd13eb7ad72aed",
       "2a22dbe3774d5b297201b55a0f17f42dce63b7845cb325cfe951d0badb5c5a14"
       "472143d896c86cc339f83671164215abc97862f2151654e75a3b357c37311b3d"
       "7268cab540202e23bee52736f2cd86cce0c7dbde95e1c600a47395dc5eb0a472"
       "153fbc4fb21b643e0c04ae14dd37e97e617a7567c89652219781001ba6f83298"},
      // PKCS#1 v1.5 Signature Example 1.11
      {"46c24e4103001629c712dd4ce8d747ee595d6c744ccc4f71347d9b8abf49d1b8"
       "fb2ef91b95dc899d4c0e3d2997e638f4cf3f68e0498de5aabd13f0dfe02ff26b"
       "a4379104e78ffa95ffbd15067ef8cbd7eb7860fecc71abe13d5c720a66851f2d"
       "efd4e795054d7bec024bb422a46a7368b56d95b47aebafbeadd612812593a70d"
       "b9f96d451ee15edb299308d777f4bb68ed3377c32156b41b7a9c92a14c8b8114"
       "4399c56a5a432f4f770aa97da8415d0bda2e813206031e70620031c881d616bf"
       "fd5f03bf147c1e73766c26246208",
       "12235b0b406126d9d260d447e923a11051fb243079f446fd73a70181d53634d7"
       "a0968e4ee27777eda63f6e4a3a91ad5985998a4848da59ce697b24bb332fa2ad"
       "9ce462ca4affdc21dab908e8ce15af6eb9105b1abcf39142aa17b34c4c092386"
       "a7abbfe028afdbebc14f2ce26fbee5edeca11502d39a6b7403154843d98a62a7"},
      // PKCS#1 v1.5 Signature Example 1.12
      {"bc99a932aa16d622bfff79c50b4c42358673261129e28d6a918ff1b0f1c4f46a"
       "d8afa98b0ca0f56f967975b0a29be882e93b6cd3fc33e1faef72e52b2ae0a3f1"
       "2024506e25690e902e782982145556532284cf505789738f4da31fa1333d3af8"
       "62b2ba6b6ce7ab4cce6aba",
       "872ec5ad4f1846256f17e9936ac50e43e9963ea8c1e76f15879b7874d77d122a"
       "609dc8c561145b94bf4ffdffdeb17e6e76ffc6c10c0747f5e37a9f434f5609e7"
       "9da5250215a457afdf12c6507cc1551f54a28010595826a2c9b97fa0aa851cc6"
       "8b705d7a06d720ba027e4a1c0b019500fb63b78071684dcfa9772700b982dc66"},
      // PKCS#1 v1.5 Signature Example 1.13
      {"731e172ac063992c5b11ba170dfb23bb000d47ba195329cf278061037381514c"
       "146064c5285db130dd5bae98b772225950eab05d3ea996f6fffb9a8c8622913f"
       "279914c89ada4f3dd77666a868bfcbff2b95b7daf453d4e2c9d75beee7f8e709"
       "05e4066a4f73aecc67f956aa5a3292b8488c917d317cfdc86253e690381e15ab",
       "76204eacc1d63ec1d6ad5bd0692e1a2f686df6e64ca945c77a824de212efa6d9"
       "782d81b4591403ff4020620298c07ebd3a8a61c5bf4dad62cbfc4ae6a03937be"
       "4b49a216d570fc6e81872937876e27bd19cf601effc30ddca573c9d56cd4569b"
       "db4851c450c42cb21e738cdd61027b8be5e9b410fc46aa3f29e4be9e64451346"},
      // PKCS#1 v1.5 Signature Example 1.14
      {"0211382683a74d8d2a2cb6a06550563be1c26ca62821e4ff163b720464fc3a28"
       "d91bedddc62749a5538eaf41fbe0c82a77e06ad99383c9e985ffb8a93fd4d7c5"
       "8db51ad91ba461d69a8fd7ddabe2496757a0c49122c1a79a85cc0553e8214d03"
       "6dfe0185efa0d05860c612fa0882c82d246e5830a67355dff18a2c36b732f988"
       "cfedc562264c6254b40fcabb97b760947568dcd6a17cda6ee8855bddbab93702"
       "471aa0cfb1bed2e13118eba1175b73c96253c108d0b2aba05ab8e17e84392e20"
       "085f47404d8365527dc3fb8f2bb48a50038e71361ccf973407",
       "525500918331f1042eae0c5c2054aa7f92deb26991b5796634f229daf9b49eb2"
       "054d87319f3cfa9b466bd075ef6699aea4bd4a195a1c52968b5e2b75e092d846"
       "ea1b5cc27905a8e1d5e5de0edfdb21391ebb951864ebd9f0b0ec35b654287136"
       "0a317b7ef13ae06af684e38e21b1e19bc7298e5d6fe0013a164bfa25d3e7313d"},
      // PKCS#1 v1.5 Signature Example 1.15
      {"fc6b700d22583388ab2f8dafcaf1a05620698020da4bae44dafbd0877b501250"
       "6dc3181d5c66bf023f348b41fd9f94795ab96452a4219f2d39d72af359cf1956"
       "51c7",
       "4452a6cc2626b01e95ab306df0d0cc7484fbab3c22e9703283567f66eadc248d"
       "bda58fce7dd0c70cce3f150fca4b369dff3b6237e2b16281ab55b53fb13089c8"
       "5cd265056b3d62a88bfc2135b16791f7fbcab9fd2dc33becb617be419d2c0461"
       "42a4d47b338314552edd4b6fe9ce1104ecec4a9958d7331e930fc09bf08a6e64"},
      // PKCS#1 v1.5 Signature Example 1.16
      {"13ba086d709cfa5fedaa557a89181a6140f2300ed6d7c3febb6cf68abebcbc67"
       "8f2bca3dc2330295eec45bb1c4075f3ada987eae88b39c51606cb80429e649d9"
       "8acc8441b1f8897db86c5a4ce0abf28b1b81dca3667697b850696b74a5ebd85d"
       "ec56c90f8abe513efa857853720be319607921bca947522cd8fac8cace5b827c"
       "3e5a129e7ee57f6b84932f14141ac4274e8cbb46e6912b0d3e2177d499d1840c"
       "d47d4d7ae0b4cdc4d3",
       "1f3b5a87db72a2c97bb3eff2a65a301268eacd89f42abc1098c1f2de77b0832a"
       "65d7815feb35070063f221bb3453bd434386c9a3fde18e3ca1687fb649e86c51"
       "d658619dde5debb86fe15491ff77ab748373f1be508880d66ea81e870e91cdf1"
       "704875c17f0b10103188bc64eef5a3551b414c733670215b1a22702562581ab1"},
      // PKCS#1 v1.5 Signature Example 1.17
      {"eb1e5935",
       "370cb9839ae6074f84b2acd6e6f6b7921b4b523463757f6446716140c4e6c0e7"
       "5bec6ad0197ebfa86bf46d094f5f6cd36dca3a5cc73c8bbb70e2c7c9ab5d964e"
       "c8e3dfde481b4a1beffd01b4ad15b31ae7aebb9b70344a9411083165fdf9c375"
       "4bbb8b94dd34bd4813dfada1f6937de4267d5597ca09a31e83d7f1a79dd19b5e"},
      // PKCS#1 v1.5 Signature Example 1.18
      {"6346b153e889c8228209630071c8a57783f368760b8eb908cfc2b276",
       "2479c975c5b1ae4c4e940f473a9045b8bf5b0bfca78ec29a38dfbedc8a749b7a"
       "2692f7c52d5bc7c831c7232372a00fed3b6b49e760ec99e074ff2eead5134e83"
       "05725dfa39212b84bd4b8d80bc8bc17a512823a3beb18fc08e45ed19c26c8177"
       "07d67fb05832ef1f12a33e90cd93b8a780319e2963ca25a2af7b09ad8f595c21"},
      // PKCS#1 v1.5 Signature Example 1.19
      {"64702db9f825a0f3abc361974659f5e9d30c3aa4f56feac69050c72905e77fe0"
       "c22f88a378c21fcf45fe8a5c717302093929",
       "152f3451c858d69594e6567dfb31291c1ee7860b9d15ebd5a5edd276ac3e6f7a"
       "8d1480e42b3381d2be023acf7ebbdb28de3d2163ae44259c6df98c335d045b61"
       "dac9dba9dbbb4e6ab4a083cd76b580cbe472206a1a9fd60680ceea1a570a29b0"
       "881c775eaef5525d6d2f344c28837d0aca422bbb0f1aba8f6861ae18bd73fe44"},
      // PKCS#1 v1.5 Signature Example 1.20
      {"941921de4a1c9c1618d6f3ca3c179f6e29bae6ddf9a6a564f929e3ce82cf3265"
       "d7837d5e692be8dcc9e86c",
       "7076c287fc6fff2b20537435e5a3107ce4da10716186d01539413e609d27d1da"
       "6fd952c61f4bab91c045fa4f8683ecc4f8dde74227f773cff3d96db84718c494"
       "4b06affeba94b725f1b07d3928b2490a85c2f1abf492a9177a7cd2ea0c966875"
       "6f825bbec900fa8ac3824e114387ef573780ca334882387b94e5aad7a27a28dc"}};

  // Import the key pair.
  blink::WebCryptoAlgorithm algorithm = CreateRsaAlgorithmWithInnerHash(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      kPublicKeySpkiDerHex,
      kPrivateKeyPkcs8DerHex,
      algorithm,
      false,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      &public_key,
      &private_key);

  // Validate the signatures are computed and verified as expected.
  blink::WebArrayBuffer signature;
  for (size_t idx = 0; idx < ARRAYSIZE_UNSAFE(kTests); ++idx) {
    SCOPED_TRACE(idx);
    const TestCase& test = kTests[idx];
    const std::vector<uint8> message = HexStringToBytes(test.message_hex);

    signature.reset();
    ASSERT_STATUS_SUCCESS(
        SignInternal(algorithm, private_key, message, &signature));
    ExpectArrayBufferMatchesHex(test.signature_hex, signature);

    bool is_match = false;
    ASSERT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        public_key,
        HexStringToBytes(test.signature_hex),
        message,
        &is_match));
    EXPECT_TRUE(is_match);
  }
}

TEST_F(WebCryptoImplTest, MAYBE(AesKwKeyImport)) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesKw);

  // Import a 128-bit Key Encryption Key (KEK)
  std::string key_raw_hex_in = "025a8cf3f08b4f6c5f33bbc76a471939";
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          HexStringToBytes(key_raw_hex_in),
                                          algorithm,
                                          true,
                                          blink::WebCryptoKeyUsageWrapKey,
                                          &key));
  blink::WebArrayBuffer key_raw_out;
  EXPECT_STATUS_SUCCESS(ExportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        key,
                        &key_raw_out));
  ExpectArrayBufferMatchesHex(key_raw_hex_in, key_raw_out);

  // Import a 192-bit KEK
  key_raw_hex_in = "c0192c6466b2370decbb62b2cfef4384544ffeb4d2fbc103";
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          HexStringToBytes(key_raw_hex_in),
                                          algorithm,
                                          true,
                                          blink::WebCryptoKeyUsageWrapKey,
                                          &key));
  EXPECT_STATUS_SUCCESS(ExportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          key,
                                          &key_raw_out));
  ExpectArrayBufferMatchesHex(key_raw_hex_in, key_raw_out);

  // Import a 256-bit Key Encryption Key (KEK)
  key_raw_hex_in =
      "e11fe66380d90fa9ebefb74e0478e78f95664d0c67ca20ce4a0b5842863ac46f";
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          HexStringToBytes(key_raw_hex_in),
                                          algorithm,
                                          true,
                                          blink::WebCryptoKeyUsageWrapKey,
                                          &key));
  EXPECT_STATUS_SUCCESS(ExportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          key,
                                          &key_raw_out));
  ExpectArrayBufferMatchesHex(key_raw_hex_in, key_raw_out);

  // Fail import of 0 length key
  EXPECT_STATUS(Status::Error(),
      ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        HexStringToBytes(""),
                        algorithm,
                        true,
                        blink::WebCryptoKeyUsageWrapKey,
                        &key));

  // Fail import of 124-bit KEK
  key_raw_hex_in = "3e4566a2bdaa10cb68134fa66c15ddb";
  EXPECT_STATUS(Status::Error(),
      ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        HexStringToBytes(key_raw_hex_in),
                        algorithm,
                        true,
                        blink::WebCryptoKeyUsageWrapKey,
                        &key));

  // Fail import of 200-bit KEK
  key_raw_hex_in = "0a1d88608a5ad9fec64f1ada269ebab4baa2feeb8d95638c0e";
  EXPECT_STATUS(Status::Error(),
        ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                          HexStringToBytes(key_raw_hex_in),
                          algorithm,
                          true,
                          blink::WebCryptoKeyUsageWrapKey,
                          &key));

  // Fail import of 260-bit KEK
  key_raw_hex_in =
      "72d4e475ff34215416c9ad9c8281247a4d730c5f275ac23f376e73e3bce8d7d5a";
  EXPECT_STATUS(Status::Error(),
      ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        HexStringToBytes(key_raw_hex_in),
                        algorithm,
                        true,
                        blink::WebCryptoKeyUsageWrapKey,
                        &key));
}

// TODO(eroman):
//   * Test decryption when the tag length exceeds input size
//   * Test decryption with empty input
//   * Test decryption with tag length of 0.
TEST_F(WebCryptoImplTest, MAYBE(AesGcmSampleSets)) {
  // Some Linux test runners may not have a new enough version of NSS.
  if (!SupportsAesGcm()) {
    LOG(WARNING) << "AES GCM not supported, skipping tests";
    return;
  }

  struct TestCase {
    const char* key;
    const char* iv;
    const char* plain_text;
    const char* cipher_text;
    const char* additional_data;
    const char* authentication_tag;
  };

  // These tests come from the NIST GCM test vectors:
  // http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmtestvectors.zip
  //
  // Both encryption and decryption are expected to work.
  TestCase kTests[] = {
    // [Keylen = 128]
    // [IVlen = 96]
    // [PTlen = 0]
    // [AADlen = 0]
    // [Taglen = 128]
    {
      // key
      "cf063a34d4a9a76c2c86787d3f96db71",
      // iv
      "113b9785971864c83b01c787",
      // plain_text
      "",
      // cipher_text
      "",
      // additional_data
      "",
      // authentication_tag
      "72ac8493e3a5228b5d130a69d2510e42",
    },

    // [Keylen = 128]
    // [IVlen = 96]
    // [PTlen = 0]
    // [AADlen = 128]
    // [Taglen = 120]
    {
      // key
      "6dfa1a07c14f978020ace450ad663d18",
      // iv
      "34edfa462a14c6969a680ec1",
      // plain_text
      "",
      // cipher_text
      "",
      // additional_data
      "2a35c7f5f8578e919a581c60500c04f6",
      // authentication_tag
      "751f3098d59cf4ea1d2fb0853bde1c"
    },

    // [Keylen = 128]
    // [IVlen = 96]
    // [PTlen = 128]
    // [AADlen = 128]
    // [Taglen = 112]
    {
      // key
      "ed6cd876ceba555706674445c229c12d",
      // iv
      "92ecbf74b765bc486383ca2e",
      // plain_text
      "bfaaaea3880d72d4378561e2597a9b35",
      // cipher_text
      "bdd2ed6c66fa087dce617d7fd1ff6d93",
      // additional_data
      "95bd10d77dbe0e87fb34217f1a2e5efe",
      // authentication_tag
      "ba82e49c55a22ed02ca67da4ec6f"
    },

    // [Keylen = 192]
    // [IVlen = 96]
    // [PTlen = 128]
    // [AADlen = 384]
    // [Taglen = 112]
    {
      // key
      "ae7972c025d7f2ca3dd37dcc3d41c506671765087c6b61b8",
      // iv
      "984c1379e6ba961c828d792d",
      // plain_text
      "d30b02c343487105219d6fa080acc743",
      // cipher_text
      "c4489fa64a6edf80e7e6a3b8855bc37c",
      // additional_data
      "edd8f630f9bbc31b0acf122998f15589d6e6e3e1a3ec89e0c6a6ece751610e"
      "bbf57fdfb9d82028ff1d9faebe37a268c1",
      // authentication_tag
      "772ee7de0f91a981c36c93a35c88"
    }
  };

  // Note that WebCrypto appends the authentication tag to the ciphertext.
  for (size_t index = 0; index < ARRAYSIZE_UNSAFE(kTests); index++) {
    SCOPED_TRACE(index);
    const TestCase& test = kTests[index];

    blink::WebCryptoKey key = ImportSecretKeyFromRawHexString(
        test.key,
        webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesGcm),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    // Verify exported raw key is identical to the imported data
    blink::WebArrayBuffer raw_key;
    EXPECT_STATUS_SUCCESS(ExportKeyInternal(
        blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatchesHex(test.key, raw_key);

    const std::vector<uint8> test_iv = HexStringToBytes(test.iv);
    const std::vector<uint8> test_additional_data =
        HexStringToBytes(test.additional_data);
    const std::vector<uint8> test_plain_text =
        HexStringToBytes(test.plain_text);
    const std::vector<uint8> test_authentication_tag =
        HexStringToBytes(test.authentication_tag);
    const unsigned test_tag_size_bits = test_authentication_tag.size() * 8;
    const std::vector<uint8> test_cipher_text =
        HexStringToBytes(test.cipher_text);

    // Test encryption.
    std::vector<uint8> cipher_text;
    std::vector<uint8> authentication_tag;
    EXPECT_STATUS_SUCCESS(AesGcmEncrypt(key, test_iv, test_additional_data,
                                        test_tag_size_bits, test_plain_text,
                                        &cipher_text, &authentication_tag));

    ExpectVectorMatchesHex(test.cipher_text, cipher_text);
    ExpectVectorMatchesHex(test.authentication_tag, authentication_tag);

    // Test decryption.
    blink::WebArrayBuffer plain_text;
    EXPECT_STATUS_SUCCESS(AesGcmDecrypt(key, test_iv, test_additional_data,
                                        test_tag_size_bits, test_cipher_text,
                                        test_authentication_tag, &plain_text));
    ExpectArrayBufferMatchesHex(test.plain_text, plain_text);

    // Decryption should fail if any of the inputs are tampered with.
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, Corrupted(test_iv), test_additional_data,
                      test_tag_size_bits, test_cipher_text,
                      test_authentication_tag, &plain_text));
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, test_iv, Corrupted(test_additional_data),
                      test_tag_size_bits, test_cipher_text,
                      test_authentication_tag, &plain_text));
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, test_iv, test_additional_data,
                      test_tag_size_bits, Corrupted(test_cipher_text),
                      test_authentication_tag, &plain_text));
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, test_iv, test_additional_data,
                      test_tag_size_bits, test_cipher_text,
                      Corrupted(test_authentication_tag),
                      &plain_text));

    // Try different incorrect tag lengths
    uint8 kAlternateTagLengths[] = {8, 96, 120, 128, 160, 255};
    for (size_t tag_i = 0; tag_i < arraysize(kAlternateTagLengths); ++tag_i) {
      unsigned wrong_tag_size_bits = kAlternateTagLengths[tag_i];
      if (test_tag_size_bits == wrong_tag_size_bits)
        continue;
      EXPECT_STATUS_ERROR(AesGcmDecrypt(key, test_iv, test_additional_data,
                                        wrong_tag_size_bits, test_cipher_text,
                                        test_authentication_tag, &plain_text));
    }
  }
}

}  // namespace content
