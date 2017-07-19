// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_expect_ct_reporter.h"

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kSendHistogramName[] = "SSL.ExpectCTReportSendingAttempt";
const char kFailureHistogramName[] = "SSL.ExpectCTReportFailure2";

// A test ReportSender that exposes the latest report URI and
// serialized report to be sent.
class TestCertificateReportSender : public net::ReportSender {
 public:
  TestCertificateReportSender()
      : ReportSender(nullptr, TRAFFIC_ANNOTATION_FOR_TESTS) {}
  ~TestCertificateReportSender() override {}

  void Send(const GURL& report_uri,
            base::StringPiece content_type,
            base::StringPiece serialized_report,
            const base::Callback<void()>& success_callback,
            const base::Callback<void(const GURL&, int, int)>& error_callback)
      override {
    latest_report_uri_ = report_uri;
    serialized_report.CopyToString(&latest_serialized_report_);
    content_type.CopyToString(&latest_content_type_);
    if (!report_callback_.is_null()) {
      EXPECT_EQ(expected_report_uri_, latest_report_uri_);
      report_callback_.Run();
    }
  }

  const GURL& latest_report_uri() const { return latest_report_uri_; }

  const std::string& latest_content_type() const {
    return latest_content_type_;
  }

  const std::string& latest_serialized_report() const {
    return latest_serialized_report_;
  }

  // Can be called to wait for a single report, which is expected to be sent to
  // |report_uri|. Returns immediately if a report has already been sent in the
  // past.
  void WaitForReport(const GURL& report_uri) {
    if (!latest_report_uri_.is_empty()) {
      EXPECT_EQ(report_uri, latest_report_uri_);
      return;
    }
    base::RunLoop run_loop;
    report_callback_ = run_loop.QuitClosure();
    expected_report_uri_ = report_uri;
    run_loop.Run();
  }

 private:
  GURL latest_report_uri_;
  std::string latest_content_type_;
  std::string latest_serialized_report_;
  base::Closure report_callback_;
  GURL expected_report_uri_;
};

// Constructs a net::SignedCertificateTimestampAndStatus with the given
// information and appends it to |sct_list|.
void MakeTestSCTAndStatus(
    net::ct::SignedCertificateTimestamp::Origin origin,
    const std::string& log_id,
    const std::string& extensions,
    const std::string& signature_data,
    const base::Time& timestamp,
    net::ct::SCTVerifyStatus status,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = log_id;
  sct->extensions = extensions;
  sct->timestamp = timestamp;
  sct->signature.signature_data = signature_data;
  sct->origin = origin;
  sct_list->push_back(net::SignedCertificateTimestampAndStatus(sct, status));
}

// Checks that |expected_cert| matches the PEM-encoded certificate chain
// in |chain|.
void CheckReportCertificateChain(
    const scoped_refptr<net::X509Certificate>& expected_cert,
    const base::ListValue& chain) {
  std::vector<std::string> pem_encoded_chain;
  expected_cert->GetPEMEncodedChain(&pem_encoded_chain);
  ASSERT_EQ(pem_encoded_chain.size(), chain.GetSize());

  for (size_t i = 0; i < pem_encoded_chain.size(); i++) {
    std::string cert_pem;
    ASSERT_TRUE(chain.GetString(i, &cert_pem));
    EXPECT_EQ(pem_encoded_chain[i], cert_pem);
  }
}

// Converts the string value of a reported SCT's origin to a
// net::ct::SignedCertificateTimestamp::Origin value.
net::ct::SignedCertificateTimestamp::Origin SCTOriginStringToOrigin(
    const std::string& origin_string) {
  if (origin_string == "embedded")
    return net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
  if (origin_string == "tls-extension")
    return net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION;
  if (origin_string == "ocsp")
    return net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
  NOTREACHED();
  return net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
}

// Checks that an SCT |sct| appears with status |status| in |report_list|, a
// list of SCTs from an Expect-CT report.
::testing::AssertionResult FindSCTInReportList(
    const scoped_refptr<net::ct::SignedCertificateTimestamp>& expected_sct,
    net::ct::SCTVerifyStatus expected_status,
    const base::ListValue& report_list) {
  std::string expected_serialized_sct;
  net::ct::EncodeSignedCertificateTimestamp(expected_sct,
                                            &expected_serialized_sct);

  for (size_t i = 0; i < report_list.GetSize(); i++) {
    const base::DictionaryValue* report_sct;
    if (!report_list.GetDictionary(i, &report_sct)) {
      return ::testing::AssertionFailure()
             << "Failed to get dictionary value from report SCT list";
    }

    std::string serialized_sct;
    EXPECT_TRUE(report_sct->GetString("serialized_sct", &serialized_sct));
    std::string decoded_serialized_sct;
    EXPECT_TRUE(base::Base64Decode(serialized_sct, &decoded_serialized_sct));
    if (decoded_serialized_sct != expected_serialized_sct)
      continue;

    std::string source;
    EXPECT_TRUE(report_sct->GetString("source", &source));
    EXPECT_EQ(expected_sct->origin, SCTOriginStringToOrigin(source));

    std::string report_status;
    EXPECT_TRUE(report_sct->GetString("status", &report_status));
    switch (expected_status) {
      case net::ct::SCT_STATUS_LOG_UNKNOWN:
        EXPECT_EQ("unknown", report_status);
        break;
      case net::ct::SCT_STATUS_INVALID_SIGNATURE:
      case net::ct::SCT_STATUS_INVALID_TIMESTAMP: {
        EXPECT_EQ("invalid", report_status);
        break;
      }
      case net::ct::SCT_STATUS_OK: {
        EXPECT_EQ("valid", report_status);
        break;
      }
      case net::ct::SCT_STATUS_NONE:
        NOTREACHED();
    }
    return ::testing::AssertionSuccess();
  }

  return ::testing::AssertionFailure() << "Failed to find SCT in report list";
}

// Checks that all |expected_scts| appears in the given lists of SCTs
// from an Expect CT report.
void CheckReportSCTs(
    const net::SignedCertificateTimestampAndStatusList& expected_scts,
    const base::ListValue& scts) {
  EXPECT_EQ(expected_scts.size(), scts.GetSize());
  for (const auto& expected_sct : expected_scts) {
    ASSERT_TRUE(
        FindSCTInReportList(expected_sct.sct, expected_sct.status, scts));
  }
}

// Checks that the |serialized_report| deserializes properly and
// contains the correct information (hostname, port, served and
// validated certificate chains, SCTs) for the given |host_port| and
// |ssl_info|.
void CheckExpectCTReport(const std::string& serialized_report,
                         const net::HostPortPair& host_port,
                         const std::string& expiration,
                         const net::SSLInfo& ssl_info) {
  std::unique_ptr<base::Value> value(base::JSONReader::Read(serialized_report));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsType(base::Value::Type::DICTIONARY));

  base::DictionaryValue* outer_report_dict;
  ASSERT_TRUE(value->GetAsDictionary(&outer_report_dict));

  base::DictionaryValue* report_dict;
  ASSERT_TRUE(
      outer_report_dict->GetDictionary("expect-ct-report", &report_dict));

  std::string report_hostname;
  EXPECT_TRUE(report_dict->GetString("hostname", &report_hostname));
  EXPECT_EQ(host_port.host(), report_hostname);
  int report_port;
  EXPECT_TRUE(report_dict->GetInteger("port", &report_port));
  EXPECT_EQ(host_port.port(), report_port);

  std::string report_expiration;
  EXPECT_TRUE(
      report_dict->GetString("effective-expiration-date", &report_expiration));
  EXPECT_EQ(expiration, report_expiration);

  const base::ListValue* report_served_certificate_chain = nullptr;
  ASSERT_TRUE(report_dict->GetList("served-certificate-chain",
                                   &report_served_certificate_chain));
  ASSERT_NO_FATAL_FAILURE(CheckReportCertificateChain(
      ssl_info.unverified_cert, *report_served_certificate_chain));

  const base::ListValue* report_validated_certificate_chain = nullptr;
  ASSERT_TRUE(report_dict->GetList("validated-certificate-chain",
                                   &report_validated_certificate_chain));
  ASSERT_NO_FATAL_FAILURE(CheckReportCertificateChain(
      ssl_info.cert, *report_validated_certificate_chain));

  const base::ListValue* report_scts = nullptr;
  ASSERT_TRUE(report_dict->GetList("scts", &report_scts));

  ASSERT_NO_FATAL_FAILURE(
      CheckReportSCTs(ssl_info.signed_certificate_timestamps, *report_scts));
}

// A test network delegate that allows the user to specify a callback to
// be run whenever a net::URLRequest is destroyed.
class TestExpectCTNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  TestExpectCTNetworkDelegate()
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

  DISALLOW_COPY_AND_ASSIGN(TestExpectCTNetworkDelegate);
};

// A test fixture that allows tests to send a report and wait until the
// net::URLRequest that sent the report is destroyed.
class ChromeExpectCTReporterWaitTest : public ::testing::Test {
 public:
  ChromeExpectCTReporterWaitTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    // Initializes URLRequestContext after the thread is set up.
    context_.reset(new net::TestURLRequestContext(true));
    context_->set_network_delegate(&network_delegate_);
    context_->Init();
    net::URLRequestFailedJob::AddUrlHandler();
  }

  void TearDown() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  net::TestURLRequestContext* context() { return context_.get(); }

 protected:
  void SendReport(ChromeExpectCTReporter* reporter,
                  const net::HostPortPair& host_port,
                  const GURL& report_uri,
                  base::Time expiration,
                  const net::SSLInfo& ssl_info) {
    base::RunLoop run_loop;
    network_delegate_.set_url_request_destroyed_callback(
        run_loop.QuitClosure());
    reporter->OnExpectCTFailed(
        host_port, report_uri, expiration, ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    run_loop.Run();
  }

 private:
  TestExpectCTNetworkDelegate network_delegate_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExpectCTReporterWaitTest);
};

// A test fixture that responds properly to CORS preflights so that reports can
// be successfully sent to test_server().
class ChromeExpectCTReporterTest : public ::testing::Test {
 public:
  ChromeExpectCTReporterTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ChromeExpectCTReporterTest() override {}

  void SetUp() override {
    report_server_.RegisterRequestHandler(
        base::Bind(&ChromeExpectCTReporterTest::HandleReportPreflight,
                   base::Unretained(this)));
    ASSERT_TRUE(report_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleReportPreflight(
      const net::test_server::HttpRequest& request) {
    handled_preflight_ = true;
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    for (const auto& cors_header : cors_headers_) {
      http_response->AddCustomHeader(cors_header.first, cors_header.second);
    }

    // If WaitForReportPreflight() has been called, signal that a preflight has
    // been handled. Do this after copying |cors_headers_| to the response,
    // because tests can mutate |cors_headers_| immediately after
    // |preflight_run_loop_| quits.
    if (preflight_run_loop_) {
      preflight_run_loop_->Quit();
    }

    return http_response;
  }

  // Can only be called once per test to wait for a single preflight.
  void WaitForReportPreflight() {
    DCHECK(!preflight_run_loop_)
        << "WaitForReportPreflight should only be called once per test";
    if (handled_preflight_) {
      return;
    }
    preflight_run_loop_ = base::MakeUnique<base::RunLoop>();
    preflight_run_loop_->Run();
  }

 protected:
  const net::EmbeddedTestServer& test_server() { return report_server_; }

  // Tests that reports are not sent when the CORS preflight request returns the
  // header field |preflight_header_name| with value given by
  // |preflight_header_bad_value|, and that reports are successfully sent when
  // it has value given by |preflight_header_good_value|.
  void TestForReportPreflightFailure(
      ChromeExpectCTReporter* reporter,
      TestCertificateReportSender* sender,
      const net::HostPortPair& host_port,
      const net::SSLInfo& ssl_info,
      const std::string& preflight_header_name,
      const std::string& preflight_header_bad_value,
      const std::string& preflight_header_good_value) {
    cors_headers_[preflight_header_name] = preflight_header_bad_value;
    const GURL fail_report_uri = test_server().GetURL("/report1");
    reporter->OnExpectCTFailed(
        host_port, fail_report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    WaitForReportPreflight();
    EXPECT_TRUE(sender->latest_report_uri().is_empty());
    EXPECT_TRUE(sender->latest_serialized_report().empty());

    // Set the proper header value and send a dummy report. The test will fail
    // if the previous OnExpectCTFailed() call unexpectedly resulted in a
    // report, as WaitForReport() would see the previous report to /report1
    // instead of the expected report to /report2.
    const GURL successful_report_uri = test_server().GetURL("/report2");
    cors_headers_[preflight_header_name] = preflight_header_good_value;
    reporter->OnExpectCTFailed(
        host_port, successful_report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    sender->WaitForReport(successful_report_uri);
    EXPECT_EQ(successful_report_uri, sender->latest_report_uri());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  net::EmbeddedTestServer report_server_;
  // Set to true when HandleReportPreflight() has been called. Used by
  // WaitForReportPreflight() to determine when to just return immediately
  // because a preflight has already been handled.
  bool handled_preflight_ = false;
  std::unique_ptr<base::RunLoop> preflight_run_loop_;
  std::map<std::string, std::string> cors_headers_{
      {"Access-Control-Allow-Origin", "*"},
      {"Access-Control-Allow-Methods", "GET,POST"},
      {"Access-Control-Allow-Headers", "content-type,another-header"}};
};

}  // namespace

// Test that no report is sent when the feature is not enabled.
TEST_F(ChromeExpectCTReporterTest, FeatureDisabled) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  net::HostPortPair host_port("example.test", 443);

  {
    const GURL report_uri = test_server().GetURL("/report1");
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(features::kExpectCTReporting);

    reporter.OnExpectCTFailed(
        host_port, report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    EXPECT_TRUE(sender->latest_report_uri().is_empty());
    EXPECT_TRUE(sender->latest_serialized_report().empty());

    histograms.ExpectTotalCount(kSendHistogramName, 0);
  }

  // Enable the feature and send a dummy report. The test will fail if the
  // previous OnExpectCTFailed() call unexpectedly resulted in a report, as the
  // WaitForReport() would see the previous report to /report1 instead of the
  // expected report to /report2.
  {
    const GURL report_uri = test_server().GetURL("/report2");
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
    reporter.OnExpectCTFailed(
        host_port, report_uri, base::Time(), ssl_info.cert.get(),
        ssl_info.unverified_cert.get(), ssl_info.signed_certificate_timestamps);
    sender->WaitForReport(report_uri);
    EXPECT_EQ(report_uri, sender->latest_report_uri());
  }
}

// Test that no report is sent if the report URI is empty.
TEST_F(ChromeExpectCTReporterTest, EmptyReportURI) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  reporter.OnExpectCTFailed(net::HostPortPair(), GURL(), base::Time(), nullptr,
                            nullptr,
                            net::SignedCertificateTimestampAndStatusList());
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  histograms.ExpectTotalCount(kSendHistogramName, 0);
}

// Test that if a report fails to send, the UMA metric is recorded.
TEST_F(ChromeExpectCTReporterWaitTest, SendReportFailure) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  ChromeExpectCTReporter reporter(context());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  net::HostPortPair host_port("example.test", 443);
  GURL report_uri(
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_FAILED));

  SendReport(&reporter, host_port, report_uri, base::Time(), ssl_info);

  histograms.ExpectTotalCount(kFailureHistogramName, 1);
  histograms.ExpectBucketCount(kFailureHistogramName,
                               -net::ERR_CONNECTION_FAILED, 1);
  histograms.ExpectTotalCount(kSendHistogramName, 1);
  histograms.ExpectBucketCount(kSendHistogramName, true, 1);
}

// Test that a sent report has the right format.
TEST_F(ChromeExpectCTReporterTest, SendReport) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::Time now = base::Time::Now();

  // Append a variety of SCTs: two of each possible status, with a
  // mixture of different origins.

  // The particular value of the log ID doesn't matter; it just has to be the
  // correct length.
  const unsigned char kTestLogId[] = {
      0xdf, 0x1c, 0x2e, 0xc1, 0x15, 0x00, 0x94, 0x52, 0x47, 0xa9, 0x61,
      0x68, 0x32, 0x5d, 0xdc, 0x5c, 0x79, 0x59, 0xe8, 0xf7, 0xc6, 0xd3,
      0x88, 0xfc, 0x00, 0x2e, 0x0b, 0xbd, 0x3f, 0x74, 0xd7, 0x01};
  const std::string log_id(reinterpret_cast<const char*>(kTestLogId),
                           sizeof(kTestLogId));
  // The values of the extensions and signature data don't matter
  // either. However, each SCT has to be unique for the test expectation to be
  // checked properly in CheckExpectCTReport(), so each SCT has a unique
  // extensions value to make sure the serialized SCTs are unique.
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions1", "signature1", now,
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions2", "signature2", now,
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, log_id,
      "extensions3", "signature1", now, net::ct::SCT_STATUS_INVALID_TIMESTAMP,
      &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION, log_id,
      "extensions4", "signature1", now, net::ct::SCT_STATUS_INVALID_SIGNATURE,
      &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions5", "signature2", now,
                       net::ct::SCT_STATUS_INVALID_SIGNATURE,
                       &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE, log_id,
      "extensions6", "signature1", now, net::ct::SCT_STATUS_OK,
      &ssl_info.signed_certificate_timestamps);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       log_id, "extensions7", "signature2", now,
                       net::ct::SCT_STATUS_OK,
                       &ssl_info.signed_certificate_timestamps);

  const char kExpirationTimeStr[] = "2017-01-01T00:00:00.000Z";
  base::Time expiration;
  ASSERT_TRUE(
      base::Time::FromUTCExploded({2017, 1, 0, 1, 0, 0, 0, 0}, &expiration));

  const GURL report_uri = test_server().GetURL("/report");

  // Check that the report is sent and contains the correct information.
  reporter.OnExpectCTFailed(net::HostPortPair::FromURL(report_uri), report_uri,
                            expiration, ssl_info.cert.get(),
                            ssl_info.unverified_cert.get(),
                            ssl_info.signed_certificate_timestamps);

  // A CORS preflight request should be sent before the actual report.
  WaitForReportPreflight();
  sender->WaitForReport(report_uri);

  EXPECT_EQ(report_uri, sender->latest_report_uri());
  EXPECT_FALSE(sender->latest_serialized_report().empty());
  EXPECT_EQ("application/expect-ct-report+json; charset=utf-8",
            sender->latest_content_type());
  ASSERT_NO_FATAL_FAILURE(CheckExpectCTReport(
      sender->latest_serialized_report(),
      net::HostPortPair::FromURL(report_uri), kExpirationTimeStr, ssl_info));

  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 1);
  histograms.ExpectBucketCount(kSendHistogramName, true, 1);
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Origin.
TEST_F(ChromeExpectCTReporterTest, BadCORSPreflightResponseOrigin) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
  EXPECT_TRUE(sender->latest_serialized_report().empty());
  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightFailure(
      &reporter, sender, net::HostPortPair("example.test", 443), ssl_info,
      "Access-Control-Allow-Origin", "https://another-origin.test", "null"));
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Methods.
TEST_F(ChromeExpectCTReporterTest, BadCORSPreflightResponseMethods) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
  EXPECT_TRUE(sender->latest_serialized_report().empty());
  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightFailure(
      &reporter, sender, net::HostPortPair("example.test", 443), ssl_info,
      "Access-Control-Allow-Methods", "GET,HEAD,POSSSST", "POST"));
}

// Test that no report is sent when the CORS preflight returns an invalid
// Access-Control-Allow-Headers.
TEST_F(ChromeExpectCTReporterTest, BadCORSPreflightResponseHeaders) {
  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.unverified_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "localhost_cert.pem");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kExpectCTReporting);
  EXPECT_TRUE(sender->latest_serialized_report().empty());
  ASSERT_NO_FATAL_FAILURE(TestForReportPreflightFailure(
      &reporter, sender, net::HostPortPair("example.test", 443), ssl_info,
      "Access-Control-Allow-Headers", "Not-Content-Type", "Content-Type"));
}
