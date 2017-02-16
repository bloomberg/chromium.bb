// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_expect_ct_reporter.h"

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
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
      : ReportSender(nullptr, net::ReportSender::DO_NOT_SEND_COOKIES) {}
  ~TestCertificateReportSender() override {}

  void Send(
      const GURL& report_uri,
      base::StringPiece content_type,
      base::StringPiece serialized_report,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(const GURL&, int)>& error_callback) override {
    latest_report_uri_ = report_uri;
    serialized_report.CopyToString(&latest_serialized_report_);
    content_type.CopyToString(&latest_content_type_);
  }

  const GURL& latest_report_uri() const { return latest_report_uri_; }

  const std::string& latest_content_type() const {
    return latest_content_type_;
  }

  const std::string& latest_serialized_report() const {
    return latest_serialized_report_;
  }

 private:
  GURL latest_report_uri_;
  std::string latest_content_type_;
  std::string latest_serialized_report_;
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
  if (origin_string == "from-tls-extension")
    return net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION;
  if (origin_string == "from-ocsp-response")
    return net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
  NOTREACHED();
  return net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
}

// Checks that an SCT |sct| appears (with the format determined by
// |status|) in |report_list|, a list of SCTs from an Expect CT
// report. |status| determines the format in that only certain fields
// are reported for certain verify statuses; SCTs from unknown logs
// contain very little information, for example, to avoid compromising
// privacy.
void FindSCTInReportList(
    const scoped_refptr<net::ct::SignedCertificateTimestamp>& sct,
    net::ct::SCTVerifyStatus status,
    const base::ListValue& report_list) {
  bool found = false;
  for (size_t i = 0; !found && i < report_list.GetSize(); i++) {
    const base::DictionaryValue* report_sct;
    ASSERT_TRUE(report_list.GetDictionary(i, &report_sct));

    std::string origin;
    ASSERT_TRUE(report_sct->GetString("origin", &origin));

    switch (status) {
      case net::ct::SCT_STATUS_LOG_UNKNOWN:
        // SCTs from unknown logs only have an origin.
        EXPECT_FALSE(report_sct->HasKey("sct"));
        EXPECT_FALSE(report_sct->HasKey("id"));
        if (SCTOriginStringToOrigin(origin) == sct->origin)
          found = true;
        break;

      case net::ct::SCT_STATUS_INVALID_SIGNATURE:
      case net::ct::SCT_STATUS_INVALID_TIMESTAMP: {
        // Invalid SCTs have a log id and an origin and nothing else.
        EXPECT_FALSE(report_sct->HasKey("sct"));
        std::string id_base64;
        ASSERT_TRUE(report_sct->GetString("id", &id_base64));
        std::string id;
        ASSERT_TRUE(base::Base64Decode(id_base64, &id));
        if (SCTOriginStringToOrigin(origin) == sct->origin && id == sct->log_id)
          found = true;
        break;
      }

      case net::ct::SCT_STATUS_OK: {
        // Valid SCTs have the full SCT.
        const base::DictionaryValue* report_sct_object;
        ASSERT_TRUE(report_sct->GetDictionary("sct", &report_sct_object));
        int version;
        ASSERT_TRUE(report_sct_object->GetInteger("sct_version", &version));
        std::string id_base64;
        ASSERT_TRUE(report_sct_object->GetString("id", &id_base64));
        std::string id;
        ASSERT_TRUE(base::Base64Decode(id_base64, &id));
        std::string extensions_base64;
        ASSERT_TRUE(
            report_sct_object->GetString("extensions", &extensions_base64));
        std::string extensions;
        ASSERT_TRUE(base::Base64Decode(extensions_base64, &extensions));
        std::string signature_data_base64;
        ASSERT_TRUE(
            report_sct_object->GetString("signature", &signature_data_base64));
        std::string signature_data;
        ASSERT_TRUE(base::Base64Decode(signature_data_base64, &signature_data));

        if (version == sct->version &&
            SCTOriginStringToOrigin(origin) == sct->origin &&
            id == sct->log_id && extensions == sct->extensions &&
            signature_data == sct->signature.signature_data) {
          found = true;
        }
        break;
      }
      default:
        NOTREACHED();
    }
  }
  EXPECT_TRUE(found);
}

// Checks that all |expected_scts| appears in the given lists of SCTs
// from an Expect CT report.
void CheckReportSCTs(
    const net::SignedCertificateTimestampAndStatusList& expected_scts,
    const base::ListValue& unknown_scts,
    const base::ListValue& invalid_scts,
    const base::ListValue& valid_scts) {
  EXPECT_EQ(
      expected_scts.size(),
      unknown_scts.GetSize() + invalid_scts.GetSize() + valid_scts.GetSize());
  for (const auto& expected_sct : expected_scts) {
    switch (expected_sct.status) {
      case net::ct::SCT_STATUS_LOG_UNKNOWN:
        ASSERT_NO_FATAL_FAILURE(FindSCTInReportList(
            expected_sct.sct, net::ct::SCT_STATUS_LOG_UNKNOWN, unknown_scts));
        break;
      case net::ct::SCT_STATUS_INVALID_SIGNATURE:
        ASSERT_NO_FATAL_FAILURE(FindSCTInReportList(
            expected_sct.sct, net::ct::SCT_STATUS_INVALID_SIGNATURE,
            invalid_scts));
        break;
      case net::ct::SCT_STATUS_INVALID_TIMESTAMP:
        ASSERT_NO_FATAL_FAILURE(FindSCTInReportList(
            expected_sct.sct, net::ct::SCT_STATUS_INVALID_TIMESTAMP,
            invalid_scts));
        break;
      case net::ct::SCT_STATUS_OK:
        ASSERT_NO_FATAL_FAILURE(FindSCTInReportList(
            expected_sct.sct, net::ct::SCT_STATUS_OK, valid_scts));
        break;
      default:
        NOTREACHED();
    }
  }
}

// Checks that the |serialized_report| deserializes properly and
// contains the correct information (hostname, port, served and
// validated certificate chains, SCTs) for the given |host_port| and
// |ssl_info|.
void CheckExpectCTReport(const std::string& serialized_report,
                         const net::HostPortPair& host_port,
                         const net::SSLInfo& ssl_info) {
  std::unique_ptr<base::Value> value(base::JSONReader::Read(serialized_report));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->IsType(base::Value::Type::DICTIONARY));

  base::DictionaryValue* report_dict;
  ASSERT_TRUE(value->GetAsDictionary(&report_dict));

  std::string report_hostname;
  EXPECT_TRUE(report_dict->GetString("hostname", &report_hostname));
  EXPECT_EQ(host_port.host(), report_hostname);
  int report_port;
  EXPECT_TRUE(report_dict->GetInteger("port", &report_port));
  EXPECT_EQ(host_port.port(), report_port);

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

  const base::ListValue* report_unknown_scts = nullptr;
  ASSERT_TRUE(report_dict->GetList("unknown-scts", &report_unknown_scts));
  const base::ListValue* report_invalid_scts = nullptr;
  ASSERT_TRUE(report_dict->GetList("invalid-scts", &report_invalid_scts));
  const base::ListValue* report_valid_scts = nullptr;
  ASSERT_TRUE(report_dict->GetList("valid-scts", &report_valid_scts));

  ASSERT_NO_FATAL_FAILURE(CheckReportSCTs(
      ssl_info.signed_certificate_timestamps, *report_unknown_scts,
      *report_invalid_scts, *report_valid_scts));
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
                  const net::SSLInfo& ssl_info) {
    base::RunLoop run_loop;
    network_delegate_.set_url_request_destroyed_callback(
        run_loop.QuitClosure());
    reporter->OnExpectCTFailed(host_port, report_uri, ssl_info);
    run_loop.Run();
  }

 private:
  TestExpectCTNetworkDelegate network_delegate_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExpectCTReporterWaitTest);
};

}  // namespace

// Test that no report is sent when the feature is not enabled.
TEST(ChromeExpectCTReporterTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kExpectCTReporting);

  base::MessageLoop message_loop;
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
  GURL report_uri("http://example-report.test");

  reporter.OnExpectCTFailed(host_port, report_uri, ssl_info);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  histograms.ExpectTotalCount(kSendHistogramName, 0);
}

// Test that no report is sent if the report URI is empty.
TEST(ChromeExpectCTReporterTest, EmptyReportURI) {
  base::MessageLoop message_loop;
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kSendHistogramName, 0);

  TestCertificateReportSender* sender = new TestCertificateReportSender();
  net::TestURLRequestContext context;
  ChromeExpectCTReporter reporter(&context);
  reporter.report_sender_.reset(sender);
  EXPECT_TRUE(sender->latest_report_uri().is_empty());
  EXPECT_TRUE(sender->latest_serialized_report().empty());

  reporter.OnExpectCTFailed(net::HostPortPair("example.test", 443), GURL(),
                            net::SSLInfo());
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

  SendReport(&reporter, host_port, report_uri, ssl_info);

  histograms.ExpectTotalCount(kFailureHistogramName, 1);
  histograms.ExpectBucketCount(kFailureHistogramName,
                               -net::ERR_CONNECTION_FAILED, 1);
  histograms.ExpectTotalCount(kSendHistogramName, 1);
  histograms.ExpectBucketCount(kSendHistogramName, true, 1);
}

// Test that a sent report has the right format.
TEST(ChromeExpectCTReporterTest, SendReport) {
  base::MessageLoop message_loop;
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

  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "unknown_log_id1", "extensions1", "signature1", now,
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "unknown_log_id2", "extensions2", "signature2", now,
                       net::ct::SCT_STATUS_LOG_UNKNOWN,
                       &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
      "invalid_log_id1", "extensions1", "signature1", now,
      net::ct::SCT_STATUS_INVALID_TIMESTAMP,
      &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION,
      "invalid_log_id1", "extensions1", "signature1", now,
      net::ct::SCT_STATUS_INVALID_SIGNATURE,
      &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "invalid_log_id2", "extensions2", "signature2", now,
                       net::ct::SCT_STATUS_INVALID_SIGNATURE,
                       &ssl_info.signed_certificate_timestamps);

  MakeTestSCTAndStatus(
      net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE,
      "valid_log_id1", "extensions1", "signature1", now, net::ct::SCT_STATUS_OK,
      &ssl_info.signed_certificate_timestamps);
  MakeTestSCTAndStatus(net::ct::SignedCertificateTimestamp::SCT_EMBEDDED,
                       "valid_log_id2", "extensions2", "signature2", now,
                       net::ct::SCT_STATUS_OK,
                       &ssl_info.signed_certificate_timestamps);

  net::HostPortPair host_port("example.test", 443);
  GURL report_uri("http://example-report.test");

  // Check that the report is sent and contains the correct information.
  reporter.OnExpectCTFailed(host_port, report_uri, ssl_info);
  EXPECT_EQ(report_uri, sender->latest_report_uri());
  EXPECT_FALSE(sender->latest_serialized_report().empty());
  EXPECT_EQ("application/json; charset=utf-8", sender->latest_content_type());
  ASSERT_NO_FATAL_FAILURE(CheckExpectCTReport(
      sender->latest_serialized_report(), host_port, ssl_info));

  histograms.ExpectTotalCount(kFailureHistogramName, 0);
  histograms.ExpectTotalCount(kSendHistogramName, 1);
  histograms.ExpectBucketCount(kSendHistogramName, true, 1);
}
