// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlstr.h>
#include <wincrypt.h>
#include <windows.h>
#include <wintrust.h>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "chrome/app/signature_validator_win.h"
#include "net/cert/test_root_certs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGoogleCertIssuer[] = "Google Inc";
const int CERT_BUFFER_SIZE = 1024;

const base::FilePath::CharType kCertificateRelativePath[] =
    FILE_PATH_LITERAL("chrome\\app\\test_data\\certificates\\");
const base::FilePath::CharType kDLLRelativePath[] =
    FILE_PATH_LITERAL("chrome\\app\\test_data\\dlls\\");

class SignatureValidatorTest : public testing::Test {
 protected:
  SignatureValidatorTest() {}

  void SetUp() OVERRIDE {
    test_roots_ = net::TestRootCerts::GetInstance();
    base::FilePath cert_path =
        GetTestCertsDirectory().Append(L"AuthorityCert.cer");
    base::FilePath other_cert_path =
        GetTestCertsDirectory().Append(L"OtherAuthorityCert.cer");
    test_roots_->AddFromFile(cert_path);
    test_roots_->AddFromFile(other_cert_path);
    EXPECT_FALSE(test_roots_->IsEmpty());

    SetExpectedHash(GetTestCertsDirectory().Append(L"ValidCert.cer"));
  }

  void TearDown() OVERRIDE {
    test_roots_->Clear();
    EXPECT_TRUE(test_roots_->IsEmpty());
  }

  base::FilePath GetTestCertsDirectory() {
    base::FilePath src_root;
    PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
    return src_root.Append(kCertificateRelativePath);
  }

  base::FilePath GetTestDLLsDirectory() {
    base::FilePath src_root;
    PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
    return src_root.Append(kDLLRelativePath);
  }

  void SetExpectedHash(const base::FilePath& cert_path) {
    char cert_buffer[CERT_BUFFER_SIZE];
    base::ReadFile(cert_path, cert_buffer, CERT_BUFFER_SIZE);

    PCCERT_CONTEXT cert = CertCreateCertificateContext(X509_ASN_ENCODING,
        reinterpret_cast<byte*>(&cert_buffer),
        CERT_BUFFER_SIZE);

    CRYPT_BIT_BLOB blob = cert->pCertInfo->SubjectPublicKeyInfo.PublicKey;
    size_t public_key_length = blob.cbData;
    uint8* public_key = blob.pbData;

    uint8 hash[crypto::kSHA256Length] = {0};

    base::StringPiece key_bytes(reinterpret_cast<char*>(public_key),
                                public_key_length);
    crypto::SHA256HashString(key_bytes, hash, crypto::kSHA256Length);

    std::string public_key_hash =
        StringToLowerASCII(base::HexEncode(hash, arraysize(hash)));
    expected_hashes_.push_back(public_key_hash);
  }

  void RunTest(const wchar_t* dll_filename, bool isValid, bool isGoogle) {
    base::FilePath full_dll_path = GetTestDLLsDirectory().Append(dll_filename);
    ASSERT_EQ(isValid, VerifyAuthenticodeSignature(full_dll_path));
    ASSERT_EQ(isGoogle, VerifySignerIsGoogle(full_dll_path, kGoogleCertIssuer,
                                             expected_hashes_));
  }

 private:
  net::TestRootCerts* test_roots_;
  std::vector<std::string> expected_hashes_;
};

}  // namespace

TEST_F(SignatureValidatorTest, ValidSigTest) {
  RunTest(L"valid_sig.dll", true, true);
}

TEST_F(SignatureValidatorTest, SelfSignedTest) {
  RunTest(L"self_signed.dll", false, false);
}

TEST_F(SignatureValidatorTest, NotSignedTest) {
  RunTest(L"not_signed.dll", false, false);
}

TEST_F(SignatureValidatorTest, NotGoogleTest) {
  RunTest(L"not_google.dll", true, false);
}

TEST_F(SignatureValidatorTest, CertPinningTest) {
  RunTest(L"different_hash.dll", true, false);
}

TEST_F(SignatureValidatorTest, ExpiredCertTest) {
  //TODO(caitkp): Figure out how to sign a dll with an expired cert.
  RunTest(L"expired.dll", false, false);
}


