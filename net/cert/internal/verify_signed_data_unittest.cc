// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_signed_data.h"

#include <set>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/pem_tokenizer.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Creates a der::Input from an std::string. The lifetimes are a bit subtle
// when using this function:
//
// The returned der::Input() is only valid so long as the input string is alive
// and is not mutated.
//
// Note that the input parameter has been made a pointer to prevent callers
// from accidentally passing an r-value.
der::Input InputFromString(const std::string* s) {
  return der::Input(reinterpret_cast<const uint8_t*>(s->data()), s->size());
}

// Reads a signature verification test file.
//
// The test file is a series of PEM blocks (PEM is just base64 data) with
// headings of:
//
//    "PUBLIC KEY" - DER encoding of the SubjectPublicKeyInfo
//    "ALGORITHM" - DER encoding of the AlgorithmIdentifier for the signature
//                  algorithm (signatureAlgorithm in X.509)
//    "DATA" - The data that was signed (tbsCertificate in X.509)
//    "SIGNATURE" - The result of signing DATA.
::testing::AssertionResult ParseTestDataFile(const std::string& file_data,
                                             std::string* public_key,
                                             std::string* algorithm,
                                             std::string* signed_data,
                                             std::string* signature_value) {
  const char kPublicKeyBlock[] = "PUBLIC KEY";
  const char kAlgorithmBlock[] = "ALGORITHM";
  const char kSignedDataBlock[] = "DATA";
  const char kSignatureBlock[] = "SIGNATURE";

  std::vector<std::string> pem_headers;
  pem_headers.push_back(kPublicKeyBlock);
  pem_headers.push_back(kAlgorithmBlock);
  pem_headers.push_back(kSignedDataBlock);
  pem_headers.push_back(kSignatureBlock);

  // Keep track of which blocks have been encountered (by elimination).
  std::set<std::string> remaining_blocks(pem_headers.begin(),
                                         pem_headers.end());

  PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    const std::string& block_type = pem_tokenizer.block_type();
    if (block_type == kPublicKeyBlock) {
      public_key->assign(pem_tokenizer.data());
    } else if (block_type == kAlgorithmBlock) {
      algorithm->assign(pem_tokenizer.data());
    } else if (block_type == kSignedDataBlock) {
      signed_data->assign(pem_tokenizer.data());
    } else if (block_type == kSignatureBlock) {
      signature_value->assign(pem_tokenizer.data());
    }

    if (remaining_blocks.erase(block_type) != 1u) {
      return ::testing::AssertionFailure()
             << "PEM block defined multiple times: " << block_type;
    }
  }

  if (!remaining_blocks.empty()) {
    // Print one of the missing PEM blocks.
    return ::testing::AssertionFailure() << "PEM block missing: "
                                         << *remaining_blocks.begin();
  }

  return ::testing::AssertionSuccess();
}

// Returns a path to the file |file_name| within the unittest data directory.
base::FilePath GetTestFilePath(const char* file_name) {
  base::FilePath src_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  return src_root.Append(
                     FILE_PATH_LITERAL("net/data/verify_signed_data_unittest"))
      .AppendASCII(file_name);
}

enum VerifyResult {
  SUCCESS,
  FAILURE,
};

// Reads test data from |file_name| and runs VerifySignedData() over its inputs.
//
// If expected_result was SUCCESS then the test will only succeed if
// VerifySignedData() returns true.
//
// If expected_result was FAILURE then the test will only succeed if
// VerifySignedData() returns false.
void RunTestCase(VerifyResult expected_result, const char* file_name) {
#if !defined(USE_OPENSSL)
  LOG(INFO) << "Skipping test, only implemented for BoringSSL";
  return;
#endif

  base::FilePath test_file_path = GetTestFilePath(file_name);

  std::string file_data;
  ASSERT_TRUE(base::ReadFileToString(test_file_path, &file_data))
      << "Couldn't read file: " << test_file_path.value();

  std::string public_key;
  std::string algorithm;
  std::string signed_data;
  std::string signature_value;

  ASSERT_TRUE(ParseTestDataFile(file_data, &public_key, &algorithm,
                                &signed_data, &signature_value));

  scoped_ptr<SignatureAlgorithm> signature_algorithm =
      SignatureAlgorithm::CreateFromDer(InputFromString(&algorithm));
  ASSERT_TRUE(signature_algorithm);

  der::BitString signature_value_bit_string;
  der::Parser signature_value_parser(InputFromString(&signature_value));
  ASSERT_TRUE(signature_value_parser.ReadBitString(&signature_value_bit_string))
      << "The signature value is not a valid BIT STRING";

  bool expected_result_bool = expected_result == SUCCESS;

  EXPECT_EQ(expected_result_bool,
            VerifySignedData(
                *signature_algorithm, InputFromString(&signed_data),
                signature_value_bit_string, InputFromString(&public_key)));
}

// Read the descriptions in the test files themselves for details on what is
// being tested.

TEST(VerifySignedDataTest, RsaPkcs1Sha1) {
  RunTestCase(SUCCESS, "rsa-pkcs1-sha1.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256) {
  RunTestCase(SUCCESS, "rsa-pkcs1-sha256.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256KeyEncodedBer) {
  // TODO(eroman): This should fail! (SPKI should be DER-encoded).
  RunTestCase(SUCCESS, "rsa-pkcs1-sha256-key-encoded-ber.pem");
}

TEST(VerifySignedDataTest, EcdsaSecp384r1Sha256) {
  RunTestCase(SUCCESS, "ecdsa-secp384r1-sha256.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512) {
  RunTestCase(SUCCESS, "ecdsa-prime256v1-sha512.pem");
}

TEST(VerifySignedDataTest, RsaPssSha1) {
  RunTestCase(SUCCESS, "rsa-pss-sha1-salt20.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256Mgf1Sha512Salt33) {
  RunTestCase(SUCCESS, "rsa-pss-sha256-mgf1-sha512-salt33.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256) {
  RunTestCase(SUCCESS, "rsa-pss-sha256-salt10.pem");
}

TEST(VerifySignedDataTest, RsaPssSha1WrongSalt) {
  RunTestCase(FAILURE, "rsa-pss-sha1-wrong-salt.pem");
}

TEST(VerifySignedDataTest, EcdsaSecp384r1Sha256CorruptedData) {
  RunTestCase(FAILURE, "ecdsa-secp384r1-sha256-corrupted-data.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1WrongAlgorithm) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-wrong-algorithm.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512WrongSignatureFormat) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-wrong-signature-format.pem");
}

TEST(VerifySignedDataTest, EcdsaUsingRsaKey) {
  RunTestCase(FAILURE, "ecdsa-using-rsa-key.pem");
}

TEST(VerifySignedDataTest, RsaUsingEcKey) {
  RunTestCase(FAILURE, "rsa-using-ec-key.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1BadKeyDerNull) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-bad-key-der-null.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1BadKeyDerLength) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-bad-key-der-length.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256UsingEcdsaAlgorithm) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha256-using-ecdsa-algorithm.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UsingRsaAlgorithm) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-rsa-algorithm.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UsingEcdhKey) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-ecdh-key.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UsingEcmqvKey) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-using-ecmqv-key.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1KeyParamsAbsent) {
  // TODO(eroman): This should fail! (key algoritm parsing is too permissive)
  RunTestCase(SUCCESS, "rsa-pkcs1-sha1-key-params-absent.pem");
}

TEST(VerifySignedDataTest, RsaPssSha1Salt20UsingPssKeyNoParams) {
  // TODO(eroman): This should pass! (rsaPss not currently supported in key
  // algorithm).
  RunTestCase(FAILURE, "rsa-pss-sha1-salt20-using-pss-key-no-params.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha1UsingPssKeyNoParams) {
  RunTestCase(FAILURE, "rsa-pkcs1-sha1-using-pss-key-no-params.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256Salt10UsingPssKeyWithParams) {
  // TODO(eroman): This should pass! (rsaPss not currently supported in key
  // algorithm).
  RunTestCase(FAILURE, "rsa-pss-sha256-salt10-using-pss-key-with-params.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256Salt10UsingPssKeyWithWrongParams) {
  RunTestCase(FAILURE,
              "rsa-pss-sha256-salt10-using-pss-key-with-wrong-params.pem");
}

TEST(VerifySignedDataTest, RsaPssSha256Salt12UsingPssKeyWithNullParams) {
  RunTestCase(FAILURE,
              "rsa-pss-sha1-salt20-using-pss-key-with-null-params.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512SpkiParamsNull) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-spki-params-null.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256UsingIdEaRsa) {
  // TODO(eroman): This should fail! (shouldn't recognize this weird OID).
  RunTestCase(SUCCESS, "rsa-pkcs1-sha256-using-id-ea-rsa.pem");
}

TEST(VerifySignedDataTest, RsaPkcs1Sha256SpkiNonNullParams) {
  // TODO(eroman): This should fail! (shouldn't recognize bogus params in rsa
  // SPKI).
  RunTestCase(SUCCESS, "rsa-pkcs1-sha256-spki-non-null-params.pem");
}

TEST(VerifySignedDataTest, EcdsaPrime256v1Sha512UnusedBitsSignature) {
  RunTestCase(FAILURE, "ecdsa-prime256v1-sha512-unused-bits-signature.pem");
}

}  // namespace

}  // namespace net
