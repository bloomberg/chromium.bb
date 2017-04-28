// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/test_helpers.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/pem_tokenizer.h"
#include "net/der/parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace der {

void PrintTo(const Input& data, ::std::ostream* os) {
  std::string b64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(data.UnsafeData()),
                        data.Length()),
      &b64);

  *os << "[" << b64 << "]";
}

}  // namespace der

der::Input SequenceValueFromString(const std::string* s) {
  der::Parser parser((der::Input(s)));
  der::Input data;
  if (!parser.ReadTag(der::kSequence, &data)) {
    ADD_FAILURE();
    return der::Input();
  }
  if (parser.HasMore()) {
    ADD_FAILURE();
    return der::Input();
  }
  return data;
}

::testing::AssertionResult ReadTestDataFromPemFile(
    const std::string& file_path_ascii,
    const PemBlockMapping* mappings,
    size_t mappings_length) {
  // Compute the full path, relative to the src/ directory.
  base::FilePath src_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  base::FilePath filepath = src_root.AppendASCII(file_path_ascii);

  // Read the full contents of the PEM file.
  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    return ::testing::AssertionFailure() << "Couldn't read file: "
                                         << filepath.value();
  }

  // mappings_copy is used to keep track of which mappings have already been
  // satisfied (by nulling the |value| field). This is used to track when
  // blocks are mulitply defined.
  std::vector<PemBlockMapping> mappings_copy(mappings,
                                             mappings + mappings_length);

  // Build the |pem_headers| vector needed for PEMTokenzier.
  std::vector<std::string> pem_headers;
  for (const auto& mapping : mappings_copy) {
    pem_headers.push_back(mapping.block_name);
  }

  PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    for (auto& mapping : mappings_copy) {
      // Find the mapping for this block type.
      if (pem_tokenizer.block_type() == mapping.block_name) {
        if (!mapping.value) {
          return ::testing::AssertionFailure()
                 << "PEM block defined multiple times: " << mapping.block_name;
        }

        // Copy the data to the result.
        mapping.value->assign(pem_tokenizer.data());

        // Mark the mapping as having been satisfied.
        mapping.value = nullptr;
      }
    }
  }

  // Ensure that all specified blocks were found.
  for (const auto& mapping : mappings_copy) {
    if (mapping.value && !mapping.optional) {
      return ::testing::AssertionFailure() << "PEM block missing: "
                                           << mapping.block_name;
    }
  }

  return ::testing::AssertionSuccess();
}

VerifyCertChainTest::VerifyCertChainTest() = default;
VerifyCertChainTest::~VerifyCertChainTest() = default;

void ReadVerifyCertChainTestFromFile(const std::string& file_path_ascii,
                                     VerifyCertChainTest* test) {
  // Reset all the out parameters to their defaults.
  *test = {};

  std::string file_data = ReadTestFileToString(file_path_ascii);

  std::vector<std::string> pem_headers;

  // For details on the file format refer to:
  // net/data/verify_certificate_chain_unittest/README.
  const char kCertificateHeader[] = "CERTIFICATE";
  const char kTrustAnchorUnconstrained[] = "TRUST_ANCHOR_UNCONSTRAINED";
  const char kTrustAnchorConstrained[] = "TRUST_ANCHOR_CONSTRAINED";
  const char kTimeHeader[] = "TIME";
  const char kResultHeader[] = "VERIFY_RESULT";
  const char kErrorsHeader[] = "ERRORS";
  const char kKeyPurpose[] = "KEY_PURPOSE";

  pem_headers.push_back(kCertificateHeader);
  pem_headers.push_back(kTrustAnchorUnconstrained);
  pem_headers.push_back(kTrustAnchorConstrained);
  pem_headers.push_back(kTimeHeader);
  pem_headers.push_back(kResultHeader);
  pem_headers.push_back(kErrorsHeader);
  pem_headers.push_back(kKeyPurpose);

  bool has_time = false;
  bool has_result = false;
  bool has_errors = false;
  bool has_key_purpose = false;
  bool has_trust_anchor = false;

  PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    const std::string& block_type = pem_tokenizer.block_type();
    const std::string& block_data = pem_tokenizer.data();

    if (block_type == kCertificateHeader) {
      ASSERT_FALSE(has_trust_anchor) << "Trust anchor must appear last";
      CertErrors errors;
      ASSERT_TRUE(net::ParsedCertificate::CreateAndAddToVector(
          bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
              reinterpret_cast<const uint8_t*>(block_data.data()),
              block_data.size(), nullptr)),
          {}, &test->chain, &errors))
          << errors.ToDebugString();
    } else if (block_type == kTrustAnchorUnconstrained ||
               block_type == kTrustAnchorConstrained) {
      ASSERT_FALSE(has_trust_anchor) << "Duplicate trust anchor";
      CertErrors errors;
      scoped_refptr<ParsedCertificate> root = net::ParsedCertificate::Create(
          bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
              reinterpret_cast<const uint8_t*>(block_data.data()),
              block_data.size(), nullptr)),
          {}, &errors);
      ASSERT_TRUE(root) << errors.ToDebugString();
      test->chain.push_back(std::move(root));
      test->last_cert_trust =
          (block_type == kTrustAnchorUnconstrained)
              ? CertificateTrust::ForTrustAnchor()
              : CertificateTrust::ForTrustAnchorEnforcingConstraints();
      has_trust_anchor = true;
    } else if (block_type == kTimeHeader) {
      ASSERT_FALSE(has_time) << "Duplicate " << kTimeHeader;
      has_time = true;
      ASSERT_TRUE(der::ParseUTCTime(der::Input(&block_data), &test->time));
    } else if (block_type == kKeyPurpose) {
      ASSERT_FALSE(has_key_purpose) << "Duplicate " << kKeyPurpose;
      has_key_purpose = true;

      if (block_data == "anyExtendedKeyUsage") {
        test->key_purpose = KeyPurpose::ANY_EKU;
      } else if (block_data == "serverAuth") {
        test->key_purpose = KeyPurpose::SERVER_AUTH;
      } else if (block_data == "clientAuth") {
        test->key_purpose = KeyPurpose::CLIENT_AUTH;
      } else {
        ADD_FAILURE() << "Unrecognized " << block_type << ": " << block_data;
      }
    } else if (block_type == kResultHeader) {
      ASSERT_FALSE(has_result) << "Duplicate " << kResultHeader;
      ASSERT_TRUE(block_data == "SUCCESS" || block_data == "FAIL")
          << "Unrecognized result: " << block_data;
      has_result = true;
      test->expected_result = block_data == "SUCCESS";
    } else if (block_type == kErrorsHeader) {
      ASSERT_FALSE(has_errors) << "Duplicate " << kErrorsHeader;
      has_errors = true;
      test->expected_errors = block_data;
    }
  }

  ASSERT_TRUE(has_time);
  ASSERT_TRUE(has_result);
  ASSERT_TRUE(has_trust_anchor);
  ASSERT_TRUE(has_key_purpose);
}

std::string ReadTestFileToString(const std::string& file_path_ascii) {
  // Compute the full path, relative to the src/ directory.
  base::FilePath src_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  base::FilePath filepath = src_root.AppendASCII(file_path_ascii);

  // Read the full contents of the file.
  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    ADD_FAILURE() << "Couldn't read file: " << filepath.value();
    return std::string();
  }

  return file_data;
}

}  // namespace net
