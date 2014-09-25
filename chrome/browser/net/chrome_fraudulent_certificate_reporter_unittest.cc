// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_fraudulent_certificate_reporter.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/request_priority.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/fraudulent_certificate_reporter.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using net::SSLInfo;

namespace chrome_browser_net {

// Builds an SSLInfo from an invalid cert chain. In this case, the cert is
// expired; what matters is that the cert would not pass even a normal
// sanity check. We test that we DO NOT send a fraudulent certificate report
// in this case.
static SSLInfo GetBadSSLInfo() {
  SSLInfo info;

  info.cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "expired_cert.pem");
  info.cert_status = net::CERT_STATUS_DATE_INVALID;
  info.is_issued_by_known_root = false;

  return info;
}

// Builds an SSLInfo from a "good" cert chain, as defined by IsGoodSSLInfo,
// but which does not pass DomainState::IsChainOfPublicKeysPermitted. In this
// case, the certificate is for mail.google.com, signed by our Chrome test
// CA. During testing, Chrome believes this CA is part of the root system
// store. But, this CA is not in the pin list; we test that we DO send a
// fraudulent certicate report in this case.
static SSLInfo GetGoodSSLInfo() {
  SSLInfo info;

  info.cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "test_mail_google_com.pem");
  info.is_issued_by_known_root = true;

  return info;
}

// Checks that |info| is good as required by the SSL checks performed in
// URLRequestHttpJob::OnStartCompleted, which are enough to trigger pin
// checking but not sufficient to pass
// DomainState::IsChainOfPublicKeysPermitted.
static bool IsGoodSSLInfo(const SSLInfo& info) {
  return info.is_valid() && info.is_issued_by_known_root;
}

class TestReporter : public ChromeFraudulentCertificateReporter {
 public:
  explicit TestReporter(net::URLRequestContext* request_context)
      : ChromeFraudulentCertificateReporter(request_context) {}
};

class SendingTestReporter : public TestReporter {
 public:
  explicit SendingTestReporter(net::URLRequestContext* request_context)
      : TestReporter(request_context), passed_(false) {}

  // Passes if invoked with a good SSLInfo and for a hostname that is a Google
  // pinned property.
  virtual void SendReport(const std::string& hostname,
                          const SSLInfo& ssl_info) OVERRIDE {
    EXPECT_TRUE(IsGoodSSLInfo(ssl_info));
    EXPECT_TRUE(net::TransportSecurityState::IsGooglePinnedProperty(hostname));
    passed_ = true;
  }

  virtual ~SendingTestReporter() {
    // If the object is destroyed without having its SendReport method invoked,
    // we failed.
    EXPECT_TRUE(passed_);
  }

  bool passed_;
};

class NotSendingTestReporter : public TestReporter {
 public:
  explicit NotSendingTestReporter(net::URLRequestContext* request_context)
      : TestReporter(request_context) {}

  // Passes if invoked with a bad SSLInfo and for a hostname that is not a
  // Google pinned property.
  virtual void SendReport(const std::string& hostname,
                          const SSLInfo& ssl_info) OVERRIDE {
    EXPECT_FALSE(IsGoodSSLInfo(ssl_info));
    EXPECT_FALSE(net::TransportSecurityState::IsGooglePinnedProperty(hostname));
  }
};

// A ChromeFraudulentCertificateReporter that uses a MockURLRequest, but is
// otherwise normal: reports are constructed and sent in the usual way.
class MockReporter : public ChromeFraudulentCertificateReporter {
 public:
  explicit MockReporter(net::URLRequestContext* request_context)
    : ChromeFraudulentCertificateReporter(request_context) {}

  virtual scoped_ptr<net::URLRequest> CreateURLRequest(
      net::URLRequestContext* context) OVERRIDE {
    return context->CreateRequest(GURL(std::string()),
                                  net::DEFAULT_PRIORITY,
                                  NULL,
                                  NULL);
  }

  virtual void SendReport(
      const std::string& hostname,
      const net::SSLInfo& ssl_info) OVERRIDE {
    DCHECK(!hostname.empty());
    DCHECK(ssl_info.is_valid());
    ChromeFraudulentCertificateReporter::SendReport(hostname, ssl_info);
  }
};

static void DoReportIsSent() {
  net::TestURLRequestContext context;
  SendingTestReporter reporter(&context);
  SSLInfo info = GetGoodSSLInfo();
  reporter.SendReport("mail.google.com", info);
}

static void DoReportIsNotSent() {
  net::TestURLRequestContext context;
  NotSendingTestReporter reporter(&context);
  SSLInfo info = GetBadSSLInfo();
  reporter.SendReport("www.example.com", info);
}

static void DoMockReportIsSent() {
  net::TestURLRequestContext context;
  MockReporter reporter(&context);
  SSLInfo info = GetGoodSSLInfo();
  reporter.SendReport("mail.google.com", info);
}

TEST(ChromeFraudulentCertificateReporterTest, GoodBadInfo) {
  SSLInfo good = GetGoodSSLInfo();
  EXPECT_TRUE(IsGoodSSLInfo(good));

  SSLInfo bad = GetBadSSLInfo();
  EXPECT_FALSE(IsGoodSSLInfo(bad));
}

TEST(ChromeFraudulentCertificateReporterTest, ReportIsSent) {
  base::MessageLoopForIO loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &loop);
  loop.PostTask(FROM_HERE, base::Bind(&DoReportIsSent));
  loop.RunUntilIdle();
}

TEST(ChromeFraudulentCertificateReporterTest, MockReportIsSent) {
  base::MessageLoopForIO loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &loop);
  loop.PostTask(FROM_HERE, base::Bind(&DoMockReportIsSent));
  loop.RunUntilIdle();
}

TEST(ChromeFraudulentCertificateReporterTest, ReportIsNotSent) {
  base::MessageLoopForIO loop;
  content::TestBrowserThread io_thread(BrowserThread::IO, &loop);
  loop.PostTask(FROM_HERE, base::Bind(&DoReportIsNotSent));
  loop.RunUntilIdle();
}

}  // namespace chrome_browser_net
