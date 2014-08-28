// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_TEST_TEST_HELPERS_H_
#define CONTENT_CHILD_WEBCRYPTO_TEST_TEST_HELPERS_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

#define EXPECT_BYTES_EQ(expected, actual) \
  EXPECT_EQ(CryptoData(expected), CryptoData(actual))

#define EXPECT_BYTES_EQ_HEX(expected_hex, actual_bytes) \
  EXPECT_BYTES_EQ(HexStringToBytes(expected_hex), actual_bytes)

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace blink {
class WebCryptoAlgorithm;
}

namespace content {

namespace webcrypto {

class Status;
class CryptoData;

// These functions are used by GTEST to support EXPECT_EQ() for
// webcrypto::Status and webcrypto::CryptoData

void PrintTo(const Status& status, ::std::ostream* os);
bool operator==(const Status& a, const Status& b);
bool operator!=(const Status& a, const Status& b);

void PrintTo(const CryptoData& data, ::std::ostream* os);
bool operator==(const CryptoData& a, const CryptoData& b);
bool operator!=(const CryptoData& a, const CryptoData& b);

// TODO(eroman): For Linux builds using system NSS, AES-GCM and RSA-OAEP, and
// RSA key import are a runtime dependency.
bool SupportsAesGcm();
bool SupportsRsaOaep();
bool SupportsRsaPrivateKeyImport();

blink::WebCryptoAlgorithm CreateRsaHashedKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId algorithm_id,
    const blink::WebCryptoAlgorithmId hash_id,
    unsigned int modulus_length,
    const std::vector<uint8_t>& public_exponent);

// Returns a slightly modified version of the input vector.
//
//  - For non-empty inputs a single bit is inverted.
//  - For empty inputs, a byte is added.
std::vector<uint8_t> Corrupted(const std::vector<uint8_t>& input);

std::vector<uint8_t> HexStringToBytes(const std::string& hex);

std::vector<uint8_t> MakeJsonVector(const std::string& json_string);
std::vector<uint8_t> MakeJsonVector(const base::DictionaryValue& dict);

// ----------------------------------------------------------------
// Helpers for working with JSON data files for test expectations.
// ----------------------------------------------------------------

// Reads a file in "src/content/test/data/webcrypto" to a base::Value.
// The file must be JSON, however it can also include C++ style comments.
::testing::AssertionResult ReadJsonTestFile(const char* test_file_name,
                                            scoped_ptr<base::Value>* value);
// Same as ReadJsonTestFile(), but returns the value as a List.
::testing::AssertionResult ReadJsonTestFileToList(
    const char* test_file_name,
    scoped_ptr<base::ListValue>* list);

// Reads a string property from the dictionary with path |property_name|
// (which can include periods for nested dictionaries). Interprets the
// string as a hex encoded string and converts it to a bytes list.
//
// Returns empty vector on failure.
std::vector<uint8_t> GetBytesFromHexString(base::DictionaryValue* dict,
                                           const char* property_name);

// Reads a string property with path "property_name" and converts it to a
// WebCryptoAlgorith. Returns null algorithm on failure.
blink::WebCryptoAlgorithm GetDigestAlgorithm(base::DictionaryValue* dict,
                                             const char* property_name);

// Returns true if any of the vectors in the input list have identical content.
bool CopiesExist(const std::vector<std::vector<uint8_t> >& bufs);

blink::WebCryptoAlgorithm CreateAesKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId aes_alg_id,
    unsigned short length);

// The following key pair is comprised of the SPKI (public key) and PKCS#8
// (private key) representations of the key pair provided in Example 1 of the
// NIST test vectors at
// ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
extern const unsigned int kModulusLengthBits;
extern const char* const kPublicKeySpkiDerHex;
extern const char* const kPrivateKeyPkcs8DerHex;

// The modulus and exponent (in hex) of kPublicKeySpkiDerHex
extern const char* const kPublicKeyModulusHex;
extern const char* const kPublicKeyExponentHex;

blink::WebCryptoKey ImportSecretKeyFromRaw(
    const std::vector<uint8_t>& key_raw,
    const blink::WebCryptoAlgorithm& algorithm,
    blink::WebCryptoKeyUsageMask usage);

void ImportRsaKeyPair(const std::vector<uint8_t>& spki_der,
                      const std::vector<uint8_t>& pkcs8_der,
                      const blink::WebCryptoAlgorithm& algorithm,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask public_key_usage_mask,
                      blink::WebCryptoKeyUsageMask private_key_usage_mask,
                      blink::WebCryptoKey* public_key,
                      blink::WebCryptoKey* private_key);

Status ImportKeyJwkFromDict(const base::DictionaryValue& dict,
                            const blink::WebCryptoAlgorithm& algorithm,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usage_mask,
                            blink::WebCryptoKey* key);

// Parses a vector of JSON into a dictionary.
scoped_ptr<base::DictionaryValue> GetJwkDictionary(
    const std::vector<uint8_t>& json);

// Verifies the input dictionary contains the expected values. Exact matches are
// required on the fields examined.
::testing::AssertionResult VerifyJwk(
    const scoped_ptr<base::DictionaryValue>& dict,
    const std::string& kty_expected,
    const std::string& alg_expected,
    blink::WebCryptoKeyUsageMask use_mask_expected);

::testing::AssertionResult VerifySecretJwk(
    const std::vector<uint8_t>& json,
    const std::string& alg_expected,
    const std::string& k_expected_hex,
    blink::WebCryptoKeyUsageMask use_mask_expected);

// Verifies that the JSON in the input vector contains the provided
// expected values. Exact matches are required on the fields examined.
::testing::AssertionResult VerifyPublicJwk(
    const std::vector<uint8_t>& json,
    const std::string& alg_expected,
    const std::string& n_expected_hex,
    const std::string& e_expected_hex,
    blink::WebCryptoKeyUsageMask use_mask_expected);

// Helper that tests importing ane exporting of symmetric keys as JWK.
void ImportExportJwkSymmetricKey(
    int key_len_bits,
    const blink::WebCryptoAlgorithm& import_algorithm,
    blink::WebCryptoKeyUsageMask usages,
    const std::string& jwk_alg);

}  // namespace webcrypto

}  // namesapce content

#endif  // CONTENT_CHILD_WEBCRYPTO_TEST_TEST_HELPERS_H_
