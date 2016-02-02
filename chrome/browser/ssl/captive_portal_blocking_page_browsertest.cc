// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_blocking_page.h"

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chrome_browser_interstitials::IsInterstitialDisplayingText;
using chrome_browser_interstitials::SecurityInterstitialIDNTest;

namespace {
// Partial text in the captive portal interstitial's main paragraph when the
// login domain isn't displayed.
const char kGenericLoginURLText[] = "its login page";
const char kBrokenSSL[] = "https://broken.ssl";
const char kWiFiSSID[] = "WiFiSSID";

enum ExpectWiFi {
  EXPECT_WIFI_NO,
  EXPECT_WIFI_YES
};

enum ExpectWiFiSSID {
  EXPECT_WIFI_SSID_NO,
  EXPECT_WIFI_SSID_YES
};

enum ExpectLoginURL {
  EXPECT_LOGIN_URL_NO,
  EXPECT_LOGIN_URL_YES
};

}  // namespace

class CaptivePortalBlockingPageForTesting : public CaptivePortalBlockingPage {
 public:
  CaptivePortalBlockingPageForTesting(
      content::WebContents* web_contents,
      const GURL& request_url,
      const GURL& login_url,
      scoped_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      const base::Callback<void(bool)>& callback,
      bool is_wifi,
      const std::string& wifi_ssid)
      : CaptivePortalBlockingPage(web_contents,
                                  request_url,
                                  login_url,
                                  std::move(ssl_cert_reporter),
                                  ssl_info,
                                  callback),
        is_wifi_(is_wifi),
        wifi_ssid_(wifi_ssid) {}

 private:
  bool IsWifiConnection() const override { return is_wifi_; }
  std::string GetWiFiSSID() const override { return wifi_ssid_; }
  const bool is_wifi_;
  const std::string wifi_ssid_;
};

class CaptivePortalBlockingPageTest
    : public certificate_reporting_test_utils::CertificateReportingTest {
 public:
  CaptivePortalBlockingPageTest() {}

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url,
                        scoped_ptr<SSLCertReporter> ssl_cert_reporter,
                        const std::string& expected_login_hostname);

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url);

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url,
                        scoped_ptr<SSLCertReporter> ssl_cert_reporter);

  void TestCertReporting(certificate_reporting_test_utils::OptIn opt_in);

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBlockingPageTest);
};

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url,
    scoped_ptr<SSLCertReporter> ssl_cert_reporter,
    const std::string& expected_login_hostname) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  net::SSLInfo ssl_info;
  ssl_info.cert = new net::X509Certificate(
      login_url.host(), "CA", base::Time::Max(), base::Time::Max());
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page =
      new CaptivePortalBlockingPageForTesting(
          contents, GURL(kBrokenSSL), login_url, std::move(ssl_cert_reporter),
          ssl_info, base::Callback<void(bool)>(), is_wifi_connection,
          wifi_ssid);
  blocking_page->Show();

  WaitForInterstitialAttach(contents);
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));
  EXPECT_EQ(expect_wifi == EXPECT_WIFI_YES,
            IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                         "Wi-Fi"));
  if (!wifi_ssid.empty()) {
    EXPECT_EQ(expect_wifi_ssid == EXPECT_WIFI_SSID_YES,
              IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                           wifi_ssid));
  }
  EXPECT_EQ(expect_login_url == EXPECT_LOGIN_URL_YES,
            IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                         expected_login_hostname));
  EXPECT_EQ(expect_login_url == EXPECT_LOGIN_URL_NO,
            IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                         kGenericLoginURLText));
}

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url) {
  TestInterstitial(is_wifi_connection, wifi_ssid, login_url, expect_wifi,
                   expect_wifi_ssid, expect_login_url, nullptr,
                   login_url.host());
}

void CaptivePortalBlockingPageTest::TestInterstitial(
    bool is_wifi_connection,
    const std::string& wifi_ssid,
    const GURL& login_url,
    ExpectWiFi expect_wifi,
    ExpectWiFiSSID expect_wifi_ssid,
    ExpectLoginURL expect_login_url,
    scoped_ptr<SSLCertReporter> ssl_cert_reporter) {
  TestInterstitial(is_wifi_connection, wifi_ssid, login_url, expect_wifi,
                   expect_wifi_ssid, expect_login_url,
                   std::move(ssl_cert_reporter), login_url.host());
}

void CaptivePortalBlockingPageTest::TestCertReporting(
    certificate_reporting_test_utils::OptIn opt_in) {
  ASSERT_NO_FATAL_FAILURE(SetUpMockReporter());

  certificate_reporting_test_utils::SetCertReportingOptIn(browser(), opt_in);
  base::RunLoop run_loop;
  scoped_ptr<SSLCertReporter> ssl_cert_reporter =
      certificate_reporting_test_utils::SetUpMockSSLCertReporter(
          &run_loop,
          opt_in == certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN
              ? certificate_reporting_test_utils::CERT_REPORT_EXPECTED
              : certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED);

  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, std::string(), kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO,
                   std::move(ssl_cert_reporter));

  EXPECT_EQ(std::string(), GetLatestHostnameReported());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetInterstitialPage()->DontProceed();

  if (opt_in == certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN) {
    // Check that the mock reporter received a request to send a report.
    run_loop.Run();
    EXPECT_EQ(GURL(kBrokenSSL).host(), GetLatestHostnameReported());
  } else {
    EXPECT_EQ(std::string(), GetLatestHostnameReported());
  }
}

// If the connection is not a Wi-Fi connection, the wired network version of the
// captive portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_LoginURL) {
  TestInterstitial(false, std::string(),
                   GURL("http://captive.portal/landing_url"), EXPECT_WIFI_NO,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
}

// Same as above, but SSID is available, so the connection should be assumed to
// be Wi-Fi.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_LoginURL_With_SSID) {
  TestInterstitial(false, kWiFiSSID, GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_YES);
}

// Same as above, expect the login URL is the same as the captive portal ping
// url (i.e. the portal intercepts requests without using HTTP redirects), in
// which case the login URL shouldn't be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(false, std::string(), kLandingUrl, EXPECT_WIFI_NO,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

// Same as above, but SSID is available, so the connection should be assumed to
// be Wi-Fi.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_NoLoginURL_With_SSID) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(false, kWiFiSSID, kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_NO);
}

// If the connection is a Wi-Fi connection, the Wi-Fi version of the captive
// portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiFi_SSID_LoginURL) {
  TestInterstitial(true, kWiFiSSID, GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_YES);
}

// Same as above, with login URL but no SSID.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiFi_NoSSID_LoginURL) {
  TestInterstitial(true, std::string(),
                   GURL("http://captive.portal/landing_url"), EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
}

// Same as above, with SSID but no login URL.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiFi_SSID_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, kWiFiSSID, kLandingUrl,
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_YES, EXPECT_LOGIN_URL_NO);
}

// Same as above, with no SSID and no login URL.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiFi_NoSSID_NoLoginURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  TestInterstitial(true, std::string(), kLandingUrl, EXPECT_WIFI_YES,
                   EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest, CertReportingOptIn) {
  // This test should only run if the Finch config is such that reports
  // will be sent when opted in. This tests that a report *is* sent when
  // the user opts in under such a Finch config, and the below test
  // tests that a report *is not* sent when the user doesn't opt in
  // (under any Finch config).
  if (certificate_reporting_test_utils::GetReportExpectedFromFinch() ==
      certificate_reporting_test_utils::CERT_REPORT_EXPECTED) {
    TestCertReporting(
        certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  }
}

IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest, CertReportingOptOut) {
  TestCertReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);
}

class CaptivePortalBlockingPageIDNTest : public SecurityInterstitialIDNTest {
 protected:
  // SecurityInterstitialIDNTest implementation
  SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo empty_ssl_info;
    // Blocking page is owned by the interstitial.
    CaptivePortalBlockingPage* blocking_page =
        new CaptivePortalBlockingPageForTesting(
            contents, GURL(kBrokenSSL), request_url, nullptr, empty_ssl_info,
            base::Callback<void(bool)>(), false, "");
    return blocking_page;
  }
};

// Test that an IDN login domain is decoded properly.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageIDNTest,
                       ShowLoginIDNIfPortalRedirectsDetectionURL) {
  EXPECT_TRUE(VerifyIDNDecoded());
}
