// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/certificate_error_report.h"

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/ssl/cert_logger.pb.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::SSLInfo;
using testing::UnorderedElementsAre;

namespace {

const char kDummyHostname[] = "dummy.hostname.com";
const char kDummyFailureLog[] = "dummy failure log";
const char kTestCertFilename[] = "test_mail_google_com.pem";

const net::CertStatus kCertStatus =
    net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_REVOKED;

const CertLoggerRequest::CertError kFirstReportedCertError =
    CertLoggerRequest::ERR_CERT_COMMON_NAME_INVALID;
const CertLoggerRequest::CertError kSecondReportedCertError =
    CertLoggerRequest::ERR_CERT_REVOKED;

// Whether to include an unverified certificate chain in the test
// SSLInfo. In production code, an unverified cert chain will not be
// present if the resource was loaded from cache.
enum UnverifiedCertChainStatus {
  INCLUDE_UNVERIFIED_CERT_CHAIN,
  EXCLUDE_UNVERIFIED_CERT_CHAIN
};

SSLInfo GetTestSSLInfo(UnverifiedCertChainStatus unverified_cert_chain_status) {
  SSLInfo info;
  info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), kTestCertFilename);
  if (unverified_cert_chain_status == INCLUDE_UNVERIFIED_CERT_CHAIN) {
    info.unverified_cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                                   kTestCertFilename);
  }
  info.is_issued_by_known_root = true;
  info.cert_status = kCertStatus;
  info.pinning_failure_log = kDummyFailureLog;
  return info;
}

std::string GetPEMEncodedChain() {
  base::FilePath cert_path =
      net::GetTestCertsDirectory().AppendASCII(kTestCertFilename);
  std::string cert_data;
  EXPECT_TRUE(base::ReadFileToString(cert_path, &cert_data));
  return cert_data;
}

// Test that a serialized CertificateErrorReport can be deserialized as
// a CertLoggerRequest protobuf (which is the format that the receiving
// server expects it in) with the right data in it.
TEST(CertificateErrorReportTest, SerializedReportAsProtobuf) {
  std::string serialized_report;
  CertificateErrorReport report(kDummyHostname,
                                GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN));
  ASSERT_TRUE(report.Serialize(&serialized_report));

  CertLoggerRequest deserialized_report;
  ASSERT_TRUE(deserialized_report.ParseFromString(serialized_report));
  EXPECT_EQ(kDummyHostname, deserialized_report.hostname());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.cert_chain());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.unverified_cert_chain());
  EXPECT_EQ(1, deserialized_report.pin().size());
  EXPECT_EQ(kDummyFailureLog, deserialized_report.pin().Get(0));

  EXPECT_THAT(
      deserialized_report.cert_error(),
      UnorderedElementsAre(kFirstReportedCertError, kSecondReportedCertError));
}

TEST(CertificateErrorReportTest,
     SerializedReportAsProtobufWithInterstitialInfo) {
  std::string serialized_report;
  // Use EXCLUDE_UNVERIFIED_CERT_CHAIN here to exercise the code path
  // where SSLInfo does not contain the unverified cert chain. (The test
  // above exercises the path where it does.)
  CertificateErrorReport report(kDummyHostname,
                                GetTestSSLInfo(EXCLUDE_UNVERIFIED_CERT_CHAIN));

  report.SetInterstitialInfo(CertificateErrorReport::INTERSTITIAL_CLOCK,
                             CertificateErrorReport::USER_PROCEEDED,
                             CertificateErrorReport::INTERSTITIAL_OVERRIDABLE);

  ASSERT_TRUE(report.Serialize(&serialized_report));

  CertLoggerRequest deserialized_report;
  ASSERT_TRUE(deserialized_report.ParseFromString(serialized_report));
  EXPECT_EQ(kDummyHostname, deserialized_report.hostname());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.cert_chain());
  EXPECT_EQ(std::string(), deserialized_report.unverified_cert_chain());
  EXPECT_EQ(1, deserialized_report.pin().size());
  EXPECT_EQ(kDummyFailureLog, deserialized_report.pin().Get(0));

  EXPECT_EQ(CertLoggerInterstitialInfo::INTERSTITIAL_CLOCK,
            deserialized_report.interstitial_info().interstitial_reason());
  EXPECT_EQ(true, deserialized_report.interstitial_info().user_proceeded());
  EXPECT_EQ(true, deserialized_report.interstitial_info().overridable());

  EXPECT_THAT(
      deserialized_report.cert_error(),
      UnorderedElementsAre(kFirstReportedCertError, kSecondReportedCertError));
}

// Test that a serialized report can be parsed.
TEST(CertificateErrorReportTest, ParseSerializedReport) {
  std::string serialized_report;
  CertificateErrorReport report(kDummyHostname,
                                GetTestSSLInfo(EXCLUDE_UNVERIFIED_CERT_CHAIN));
  EXPECT_EQ(kDummyHostname, report.hostname());
  ASSERT_TRUE(report.Serialize(&serialized_report));

  CertificateErrorReport parsed;
  ASSERT_TRUE(parsed.InitializeFromString(serialized_report));
  EXPECT_EQ(report.hostname(), parsed.hostname());
}

}  // namespace
