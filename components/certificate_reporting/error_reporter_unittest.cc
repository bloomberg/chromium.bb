// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_reporting/error_reporter.h"

#include <stdint.h>
#include <string.h>

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "net/http/http_status_code.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace certificate_reporting {

namespace {

static const char kHkdfLabel[] = "certificate report";
const char kDummyHttpReportUri[] = "http://example.test";
const char kDummyHttpsReportUri[] = "https://example.test";
const char kDummyReport[] = "a dummy report";
const uint32_t kServerPublicKeyTestVersion = 16;

void ErrorCallback(bool* called,
                   const GURL& report_uri,
                   int net_error,
                   int http_response_code) {
  EXPECT_NE(net::OK, net_error);
  EXPECT_EQ(-1, http_response_code);
  *called = true;
}

void SuccessCallback(bool* called) {
  *called = true;
}

// A mock ReportSender that keeps track of the last report
// sent.
class MockCertificateReportSender : public net::ReportSender {
 public:
  MockCertificateReportSender()
      : net::ReportSender(nullptr, TRAFFIC_ANNOTATION_FOR_TESTS) {}
  ~MockCertificateReportSender() override {}

  void Send(const GURL& report_uri,
            base::StringPiece content_type,
            base::StringPiece report,
            const base::Callback<void()>& success_callback,
            const base::Callback<void(const GURL&, int, int)>& error_callback)
      override {
    latest_report_uri_ = report_uri;
    report.CopyToString(&latest_report_);
    content_type.CopyToString(&latest_content_type_);
  }

  const GURL& latest_report_uri() const { return latest_report_uri_; }

  const std::string& latest_report() const { return latest_report_; }

  const std::string& latest_content_type() const {
    return latest_content_type_;
  }

 private:
  GURL latest_report_uri_;
  std::string latest_report_;
  std::string latest_content_type_;

  DISALLOW_COPY_AND_ASSIGN(MockCertificateReportSender);
};

// A test network delegate that allows the user to specify a callback to
// be run whenever a net::URLRequest is destroyed.
class TestCertificateReporterNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  TestCertificateReporterNetworkDelegate()
      : url_request_destroyed_callback_(base::Bind(&base::DoNothing)) {}

  void set_url_request_destroyed_callback(const base::Closure& callback) {
    url_request_destroyed_callback_ = callback;
  }

  // net::NetworkDelegateImpl:
  void OnURLRequestDestroyed(net::URLRequest* request) override {
    url_request_destroyed_callback_.Run();
  }

 private:
  base::Closure url_request_destroyed_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestCertificateReporterNetworkDelegate);
};

class ErrorReporterTest : public ::testing::Test {
 public:
  ErrorReporterTest() {
    memset(server_private_key_, 1, sizeof(server_private_key_));
    X25519_public_from_private(server_public_key_, server_private_key_);
  }

  ~ErrorReporterTest() override {}

 protected:
  base::MessageLoopForIO loop_;
  uint8_t server_public_key_[32];
  uint8_t server_private_key_[32];

  DISALLOW_COPY_AND_ASSIGN(ErrorReporterTest);
};

// Test that ErrorReporter::SendExtendedReportingReport sends
// an encrypted or plaintext extended reporting report as appropriate.
TEST_F(ErrorReporterTest, ExtendedReportingSendReport) {
  // Data should not be encrypted when sent to an HTTPS URL.
  MockCertificateReportSender* mock_report_sender =
      new MockCertificateReportSender();
  GURL https_url(kDummyHttpsReportUri);
  ErrorReporter https_reporter(https_url, server_public_key_,
                               kServerPublicKeyTestVersion,
                               base::WrapUnique(mock_report_sender));
  https_reporter.SendExtendedReportingReport(
      kDummyReport, base::Callback<void()>(),
      base::Callback<void(const GURL&, int, int)>());
  EXPECT_EQ(mock_report_sender->latest_report_uri(), https_url);
  EXPECT_EQ(mock_report_sender->latest_report(), kDummyReport);

  // Data should be encrypted when sent to an HTTP URL.
  MockCertificateReportSender* http_mock_report_sender =
      new MockCertificateReportSender();
  const GURL http_url(kDummyHttpReportUri);
  ErrorReporter http_reporter(http_url, server_public_key_,
                              kServerPublicKeyTestVersion,
                              base::WrapUnique(http_mock_report_sender));
  http_reporter.SendExtendedReportingReport(
      kDummyReport, base::Callback<void()>(),
      base::Callback<void(const GURL&, int, int)>());

  EXPECT_EQ(http_mock_report_sender->latest_report_uri(), http_url);
  EXPECT_EQ("application/octet-stream",
            http_mock_report_sender->latest_content_type());

  std::string uploaded_report;
  encrypted_messages::EncryptedMessage encrypted_report;
  ASSERT_TRUE(encrypted_report.ParseFromString(
      http_mock_report_sender->latest_report()));
  EXPECT_EQ(kServerPublicKeyTestVersion,
            encrypted_report.server_public_key_version());
  EXPECT_EQ(
      encrypted_messages::EncryptedMessage::AEAD_ECDH_AES_128_CTR_HMAC_SHA256,
      encrypted_report.algorithm());
  // TODO(estark): kHkdfLabel needs to include the null character in the label
  // due to a matching error in the server for the case of certificate
  // reporting, the strlen + 1 can be removed once that error is fixed.
  // https://crbug.com/517746
  ASSERT_TRUE(encrypted_messages::DecryptMessageForTesting(
      server_private_key_,
      base::StringPiece(kHkdfLabel, strlen(kHkdfLabel) + 1), encrypted_report,
      &uploaded_report));

  EXPECT_EQ(kDummyReport, uploaded_report);
}

// Tests that an UMA histogram is recorded if a report fails to send.
TEST_F(ErrorReporterTest, ErroredRequestCallsCallback) {
  net::URLRequestFailedJob::AddUrlHandler();

  base::RunLoop run_loop;
  net::TestURLRequestContext context(true);
  TestCertificateReporterNetworkDelegate test_delegate;
  test_delegate.set_url_request_destroyed_callback(run_loop.QuitClosure());
  context.set_network_delegate(&test_delegate);
  context.Init();

  const GURL report_uri(
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_FAILED));
  ErrorReporter reporter(&context, report_uri);

  bool error_callback_called = false;
  bool success_callback_called = false;
  reporter.SendExtendedReportingReport(
      kDummyReport, base::Bind(&SuccessCallback, &success_callback_called),
      base::Bind(&ErrorCallback, &error_callback_called));
  run_loop.Run();

  EXPECT_TRUE(error_callback_called);
  EXPECT_FALSE(success_callback_called);
}

// Tests that an UMA histogram is recorded if a report is successfully sent.
TEST_F(ErrorReporterTest, SuccessfulRequestCallsCallback) {
  net::URLRequestMockDataJob::AddUrlHandler();

  base::RunLoop run_loop;
  net::TestURLRequestContext context(true);
  TestCertificateReporterNetworkDelegate test_delegate;
  test_delegate.set_url_request_destroyed_callback(run_loop.QuitClosure());
  context.set_network_delegate(&test_delegate);
  context.Init();

  const GURL report_uri(
      net::URLRequestMockDataJob::GetMockHttpUrl("some data", 1));
  ErrorReporter reporter(&context, report_uri);

  bool error_callback_called = false;
  bool success_callback_called = false;
  reporter.SendExtendedReportingReport(
      kDummyReport, base::Bind(&SuccessCallback, &success_callback_called),
      base::Bind(&ErrorCallback, &error_callback_called));
  run_loop.Run();

  EXPECT_FALSE(error_callback_called);
  EXPECT_TRUE(success_callback_called);
}

}  // namespace

}  // namespace certificate_reporting
