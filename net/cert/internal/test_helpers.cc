// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/test_helpers.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "net/cert/internal/cert_error_params.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/pem.h"
#include "net/der/parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace {

bool GetValue(base::StringPiece prefix,
              base::StringPiece line,
              std::string* value,
              bool* has_value) {
  if (!line.starts_with(prefix))
    return false;

  if (*has_value) {
    ADD_FAILURE() << "Duplicated " << prefix;
    return false;
  }

  *has_value = true;
  *value = line.substr(prefix.size()).as_string();
  return true;
}

}  // namespace

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
  base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  base::FilePath filepath = src_root.AppendASCII(file_path_ascii);

  // Read the full contents of the PEM file.
  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    return ::testing::AssertionFailure()
           << "Couldn't read file: " << filepath.value();
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
      return ::testing::AssertionFailure()
             << "PEM block missing: " << mapping.block_name;
    }
  }

  return ::testing::AssertionSuccess();
}

VerifyCertChainTest::VerifyCertChainTest()
    : user_initial_policy_set{AnyPolicy()} {}
VerifyCertChainTest::~VerifyCertChainTest() = default;

bool VerifyCertChainTest::HasHighSeverityErrors() const {
  // This function assumes that high severity warnings are prefixed with
  // "ERROR: " and warnings are prefixed with "WARNING: ". This is an
  // implementation detail of CertError::ToDebugString).
  //
  // Do a quick sanity-check to confirm this.
  CertError error(CertError::SEVERITY_HIGH, "unused", nullptr);
  EXPECT_EQ("ERROR: unused\n", error.ToDebugString());
  CertError warning(CertError::SEVERITY_WARNING, "unused", nullptr);
  EXPECT_EQ("WARNING: unused\n", warning.ToDebugString());

  // Do a simple substring test (not perfect, but good enough for our test
  // corpus).
  return expected_errors.find("ERROR: ") != std::string::npos;
}

bool ReadCertChainFromFile(const std::string& file_path_ascii,
                           ParsedCertificateList* chain) {
  // Reset all the out parameters to their defaults.
  *chain = ParsedCertificateList();

  std::string file_data = ReadTestFileToString(file_path_ascii);
  if (file_data.empty())
    return false;

  std::vector<std::string> pem_headers = {"CERTIFICATE"};

  PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    const std::string& block_data = pem_tokenizer.data();

    CertErrors errors;
    if (!ParsedCertificate::CreateAndAddToVector(
            bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
                reinterpret_cast<const uint8_t*>(block_data.data()),
                block_data.size(), nullptr)),
            {}, chain, &errors)) {
      ADD_FAILURE() << errors.ToDebugString();
      return false;
    }
  }

  return true;
}

scoped_refptr<ParsedCertificate> ReadCertFromFile(
    const std::string& file_path_ascii) {
  ParsedCertificateList chain;
  if (!ReadCertChainFromFile(file_path_ascii, &chain))
    return nullptr;
  if (chain.size() != 1)
    return nullptr;
  return chain[0];
}

bool ReadVerifyCertChainTestFromFile(const std::string& file_path_ascii,
                                     VerifyCertChainTest* test) {
  // Reset all the out parameters to their defaults.
  *test = {};

  std::string file_data = ReadTestFileToString(file_path_ascii);
  if (file_data.empty())
    return false;

  std::vector<std::string> lines = base::SplitString(
      file_data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  bool has_chain = false;
  bool has_trust = false;
  bool has_time = false;
  bool has_errors = false;
  bool has_key_purpose = false;

  base::StringPiece kExpectedErrors = "expected_errors:";

  for (const std::string& line : lines) {
    base::StringPiece line_piece(line);

    std::string value;

    // For details on the file format refer to:
    // net/data/verify_certificate_chain_unittest/README.
    if (GetValue("chain: ", line_piece, &value, &has_chain)) {
      // Interpret the |chain| path as being relative to the .test file.
      size_t slash = file_path_ascii.rfind('/');
      if (slash == std::string::npos) {
        ADD_FAILURE() << "Bad path - expecting slashes";
        return false;
      }
      std::string chain_path = file_path_ascii.substr(0, slash) + "/" + value;

      ReadCertChainFromFile(chain_path, &test->chain);
    } else if (GetValue("utc_time: ", line_piece, &value, &has_time)) {
      if (value == "DEFAULT") {
        value = "180510120000Z";
      }
      if (!der::ParseUTCTime(der::Input(&value), &test->time)) {
        ADD_FAILURE() << "Failed parsing UTC time";
        return false;
      }
    } else if (GetValue("key_purpose: ", line_piece, &value,
                        &has_key_purpose)) {
      if (value == "ANY_EKU") {
        test->key_purpose = KeyPurpose::ANY_EKU;
      } else if (value == "SERVER_AUTH") {
        test->key_purpose = KeyPurpose::SERVER_AUTH;
      } else if (value == "CLIENT_AUTH") {
        test->key_purpose = KeyPurpose::CLIENT_AUTH;
      } else {
        ADD_FAILURE() << "Unrecognized key_purpose: " << value;
        return false;
      }
    } else if (GetValue("last_cert_trust: ", line_piece, &value, &has_trust)) {
      if (value == "TRUSTED_ANCHOR") {
        test->last_cert_trust = CertificateTrust::ForTrustAnchor();
      } else if (value == "TRUSTED_ANCHOR_WITH_CONSTRAINTS") {
        test->last_cert_trust =
            CertificateTrust::ForTrustAnchorEnforcingConstraints();
      } else if (value == "DISTRUSTED") {
        test->last_cert_trust = CertificateTrust::ForDistrusted();
      } else if (value == "UNSPECIFIED") {
        test->last_cert_trust = CertificateTrust::ForUnspecified();
      } else {
        ADD_FAILURE() << "Unrecognized last_cert_trust: " << value;
        return false;
      }
    } else if (line_piece.starts_with("#")) {
      // Skip comments.
      continue;
    } else if (line_piece == kExpectedErrors) {
      has_errors = true;
      // The errors start on the next line, and extend until the end of the
      // file.
      std::string prefix =
          std::string("\n") + kExpectedErrors.as_string() + std::string("\n");
      size_t errors_start = file_data.find(prefix);
      if (errors_start == std::string::npos) {
        ADD_FAILURE() << "expected_errors not found";
        return false;
      }
      test->expected_errors = file_data.substr(errors_start + prefix.size());
      break;
    } else {
      ADD_FAILURE() << "Unknown line: " << line_piece;
      return false;
    }
  }

  if (!has_chain) {
    ADD_FAILURE() << "Missing chain: ";
    return false;
  }

  if (!has_trust) {
    ADD_FAILURE() << "Missing last_cert_trust: ";
    return false;
  }

  if (!has_time) {
    ADD_FAILURE() << "Missing time: ";
    return false;
  }

  if (!has_key_purpose) {
    ADD_FAILURE() << "Missing key_purpose: ";
    return false;
  }

  if (!has_errors) {
    ADD_FAILURE() << "Missing errors:";
    return false;
  }

  return true;
}

std::string ReadTestFileToString(const std::string& file_path_ascii) {
  // Compute the full path, relative to the src/ directory.
  base::FilePath src_root;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  base::FilePath filepath = src_root.AppendASCII(file_path_ascii);

  // Read the full contents of the file.
  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    ADD_FAILURE() << "Couldn't read file: " << filepath.value();
    return std::string();
  }

  return file_data;
}

void VerifyCertPathErrors(const std::string& expected_errors_str,
                          const CertPathErrors& actual_errors,
                          const ParsedCertificateList& chain,
                          const std::string& errors_file_path) {
  std::string actual_errors_str = actual_errors.ToDebugString(chain);

  if (expected_errors_str != actual_errors_str) {
    ADD_FAILURE() << "Cert path errors don't match expectations ("
                  << errors_file_path << ")\n\n"
                  << "EXPECTED:\n\n"
                  << expected_errors_str << "\n"
                  << "ACTUAL:\n\n"
                  << actual_errors_str << "\n"
                  << "===> Use "
                     "net/data/verify_certificate_chain_unittest/"
                     "rebase-errors.py to rebaseline.\n";
  }
}

void VerifyCertErrors(const std::string& expected_errors_str,
                      const CertErrors& actual_errors,
                      const std::string& errors_file_path) {
  std::string actual_errors_str = actual_errors.ToDebugString();

  if (expected_errors_str != actual_errors_str) {
    ADD_FAILURE() << "Cert errors don't match expectations ("
                  << errors_file_path << ")\n\n"
                  << "EXPECTED:\n\n"
                  << expected_errors_str << "\n"
                  << "ACTUAL:\n\n"
                  << actual_errors_str << "\n"
                  << "===> Use "
                     "net/data/parse_certificate_unittest/"
                     "rebase-errors.py to rebaseline.\n";
  }
}

}  // namespace net
