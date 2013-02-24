// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_util.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "net/base/x509_cert_types.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SignatureUtilWinTest : public testing::Test {
 protected:
  virtual void SetUp() {
    base::FilePath source_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
    testdata_path_ = source_path
        .AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("safe_browsing")
        .AppendASCII("download_protection");
  }

  // Given a certificate chain protobuf, parse it into X509Certificates.
  void ParseCertificateChain(
      const ClientDownloadRequest_CertificateChain& chain,
      std::vector<scoped_refptr<net::X509Certificate> >* certs) {
    for (int i = 0; i < chain.element_size(); ++i) {
      certs->push_back(
          net::X509Certificate::CreateFromBytes(
              chain.element(i).certificate().data(),
              chain.element(i).certificate().size()));
    }
  }

  base::FilePath testdata_path_;
};

TEST_F(SignatureUtilWinTest, UntrustedSignedBinary) {
  // signed.exe is signed by an untrusted root CA.
  scoped_refptr<SignatureUtil> signature_util(new SignatureUtil());
  ClientDownloadRequest_SignatureInfo signature_info;
  signature_util->CheckSignature(testdata_path_.Append(L"signed.exe"),
                                 &signature_info);
  ASSERT_EQ(1, signature_info.certificate_chain_size());
  std::vector<scoped_refptr<net::X509Certificate> > certs;
  ParseCertificateChain(signature_info.certificate_chain(0), &certs);
  ASSERT_EQ(2, certs.size());
  EXPECT_EQ("Joe's-Software-Emporium", certs[0]->subject().common_name);
  EXPECT_EQ("Root Agency", certs[1]->subject().common_name);

  EXPECT_TRUE(signature_info.has_trusted());
  EXPECT_FALSE(signature_info.trusted());
}

TEST_F(SignatureUtilWinTest, TrustedBinary) {
  // wow_helper.exe is signed using Google's signing certifiacte.
  scoped_refptr<SignatureUtil> signature_util(new SignatureUtil());
  ClientDownloadRequest_SignatureInfo signature_info;
  signature_util->CheckSignature(testdata_path_.Append(L"wow_helper.exe"),
                                 &signature_info);
  ASSERT_EQ(1, signature_info.certificate_chain_size());
  std::vector<scoped_refptr<net::X509Certificate> > certs;
  ParseCertificateChain(signature_info.certificate_chain(0), &certs);
  ASSERT_EQ(3, certs.size());

  EXPECT_EQ("Google Inc", certs[0]->subject().common_name);
  EXPECT_EQ("VeriSign Class 3 Code Signing 2009-2 CA",
            certs[1]->subject().common_name);
  EXPECT_EQ("Class 3 Public Primary Certification Authority",
            certs[2]->subject().organization_unit_names[0]);

  EXPECT_TRUE(signature_info.trusted());
}

TEST_F(SignatureUtilWinTest, UnsignedBinary) {
  // unsigned.exe has no signature information.
  scoped_refptr<SignatureUtil> signature_util(new SignatureUtil());
  ClientDownloadRequest_SignatureInfo signature_info;
  signature_util->CheckSignature(testdata_path_.Append(L"unsigned.exe"),
                                 &signature_info);
  EXPECT_EQ(0, signature_info.certificate_chain_size());
  EXPECT_FALSE(signature_info.has_trusted());
}

TEST_F(SignatureUtilWinTest, NonExistentBinary) {
  // Test a file that doesn't exist.
  scoped_refptr<SignatureUtil> signature_util(new SignatureUtil());
  ClientDownloadRequest_SignatureInfo signature_info;
  signature_util->CheckSignature(testdata_path_.Append(L"doesnotexist.exe"),
                                 &signature_info);
  EXPECT_EQ(0, signature_info.certificate_chain_size());
  EXPECT_FALSE(signature_info.has_trusted());
}

}  // namespace safe_browsing
