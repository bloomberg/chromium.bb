// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "chrome/common/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCertDateErrorHistogram[] =
    "interstitial.ssl_error_handler.cert_date_error_delay";

const net::SHA256HashValue kCertPublicKeyHashValue = {{0x01, 0x02}};

const char kOkayCertName[] = "ok_cert.pem";

// These certificates are self signed certificates with relevant issuer common
// names generated using the following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes

// Common name: "Outdated Antivirus"
const char kOutdatedAntivirusCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIID/zCCAuegAwIBAgIJANADVqnC0vPYMA0GCSqGSIb3DQEBCwUAMIGVMQswCQYD\n"
    "VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNU2FuIEZyYW5j\n"
    "aXNjbzEbMBkGA1UECgwST3V0ZGF0ZWQgQW50aXZpcnVzMRswGQYDVQQDDBJPdXRk\n"
    "YXRlZCBBbnRpdmlydXMxHzAdBgkqhkiG9w0BCQEWEHRlc3RAZXhhbXBsZS5jb20w\n"
    "HhcNMTcwODE3MDYxNjIzWhcNMTgwODE3MDYxNjIzWjCBlTELMAkGA1UEBhMCVVMx\n"
    "EzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDVNhbiBGcmFuY2lzY28xGzAZ\n"
    "BgNVBAoMEk91dGRhdGVkIEFudGl2aXJ1czEbMBkGA1UEAwwST3V0ZGF0ZWQgQW50\n"
    "aXZpcnVzMR8wHQYJKoZIhvcNAQkBFhB0ZXN0QGV4YW1wbGUuY29tMIIBIjANBgkq\n"
    "hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApE60ozR0GRvhC8pLgvQiJGnZ+QNVq2+v\n"
    "r0u8U4Gc9Rrau19owmik9N6SfViwgD8F335Ayo2Q9lF/M39fwH3fL/Ac2zHCD2Cs\n"
    "u1YOD1DEFYFzV2mkWOUHFDpVSjJVVB0Bk9b2TscOrzn5Yn+uTMuFepwwRA14ljWS\n"
    "l2spCM0LNwXBmPv3c1KI8WOTsPuLCSCgcPejQVPHhzALg9ymODPkX/zyYQKK4PzY\n"
    "dCpg1WzgScERHLTQhKrq26bWeVcJwmQ42Ea4JQ6qnOHy/yzXm786rDFDlJLpLygo\n"
    "nqERSwSpu8Up9tOp72Zo9OB05OjsneEp4W8xlVdH8cQsmBj4uztL6QIDAQABo1Aw\n"
    "TjAdBgNVHQ4EFgQUeta5n0X4vG8ph9U+6/irRxgVocEwHwYDVR0jBBgwFoAUeta5\n"
    "n0X4vG8ph9U+6/irRxgVocEwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEAowqui84tdVS9yDigrfKAFqqD5fZKpKbpRxi5aRKoaX6hOUNHSFA+pC9Xd4Sg\n"
    "W1Q1sd7rSKLG9jh3aKwhDvRFvVvKePxIPjB9f9mamt5DXH1tYuz1SylOH8DtYs50\n"
    "/9IDkaFQ8XryqSVDVZj1rpnj6pPPV00AfhVgqG1tgHzYqB7jtXoHqn/bKQX8eYZT\n"
    "VtlyGhJIKpFAiv20Pfx3Af1aOVvfyg9cM117BjZ54GyA5Md8D7CXv6G/g4p10Q+a\n"
    "LezwUjncyY0gLGm39RtPvUKHQi2i9NCfukrtKioixjvHsxQ6G2+o1np5PlO8/Xmg\n"
    "b6PrWTR/FHT44s5H8bCUHDfrCQ==\n"
    "-----END CERTIFICATE-----";

// Common name: "Misconfigured Firewall_4GHPOS5412EF"
const char kMisconfiguredFirewallCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEJTCCAw2gAwIBAgIJAO0EVfP6VLU9MA0GCSqGSIb3DQEBCwUAMIGoMQswCQYD\n"
    "VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNU2FuIEZyYW5j\n"
    "aXNjbzEfMB0GA1UECgwWTWlzY29uZmlndXJlZCBGaXJld2FsbDEqMCgGA1UEAwwh\n"
    "TWlzY29uZmlndXJlZCBGaXJld2FsbF8xR0gzNUZJTzMyMR8wHQYJKoZIhvcNAQkB\n"
    "FhB0ZXN0QGV4YW1wbGUuY29tMB4XDTE3MDgwOTE3NDgxNloXDTE4MDgwOTE3NDgx\n"
    "NlowgagxCzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQH\n"
    "DA1TYW4gRnJhbmNpc2NvMR8wHQYDVQQKDBZNaXNjb25maWd1cmVkIEZpcmV3YWxs\n"
    "MSowKAYDVQQDDCFNaXNjb25maWd1cmVkIEZpcmV3YWxsXzFHSDM1RklPMzIxHzAd\n"
    "BgkqhkiG9w0BCQEWEHRlc3RAZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUA\n"
    "A4IBDwAwggEKAoIBAQDXazq1NGo2TOr7OIFi14KnBBBA9rJ3HXH78/gDUj7B+/ji\n"
    "EB2+j1tNtdPHf7uaz0O6X3PKDeed5GV+C37khr7J+12NkIQqH2TdpXZ1rHvbjV8D\n"
    "L/NUERwoGR0+xKS9cQdMYHGyUzPkOeTg+/UKQGpeUGfFbWJVPyxjIGZ1GFKkzstX\n"
    "CLMv9lKFgU9Q0b6WNRyfvMt7ofTxkUuyTgzoN/M2WRzj8PPczYGhopsMIpQie8c5\n"
    "ziX3VQTKTPyEVwAat7uNVaKi8nza02hEahGFvv5oUiyi1dVfsSCDvzL9IeNtvO1w\n"
    "G/ooZrfqBsH45oa7kigzntwnsf0fb7Op0S1RDPnDAgMBAAGjUDBOMB0GA1UdDgQW\n"
    "BBTkWcrjWP6EivGW0GdJbZikRh/AZTAfBgNVHSMEGDAWgBTkWcrjWP6EivGW0GdJ\n"
    "bZikRh/AZTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQA1ZSnE+ntm\n"
    "2KTfmT3Xj9cWezTPWD2kc+tVSfuFCNqFBlZE586wmmlPXzkiI3UTOs0BJvoIEZYb\n"
    "yiuJW3xPe9ZVsL8y8d8z1miUIcYt1EVpoF3CxPWI0iilXoF/6GbDtmdzFk990fSO\n"
    "oGjq1Zadc4E/wxkHcYChlkIUG94WemkFRBcYmabxDJOgEGK+KDI/2cy5OlqZReBM\n"
    "o82wrUg88gNy2IdgT7LAh8nLoEwxDEE0nmawV7FXpJdEZzKlXrAidRtbrsC2hyDa\n"
    "N/rrXaEJK0iLuDxeBH6d4cu+MSho5z2paevIJSIyX8SQfVFCwjTEqf06OE02a94T\n"
    "D+5GEBRIUV20\n"
    "-----END CERTIFICATE-----";

// Runs |quit_closure| on the UI thread once a URL request has been
// seen. Returns a request that hangs.
std::unique_ptr<net::test_server::HttpResponse> WaitForRequest(
    const base::Closure& quit_closure,
    const net::test_server::HttpRequest& request) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   quit_closure);
  return base::MakeUnique<net::test_server::HungResponse>();
}

class TestSSLErrorHandler : public SSLErrorHandler {
 public:
  TestSSLErrorHandler(
      std::unique_ptr<Delegate> delegate,
      content::WebContents* web_contents,
      Profile* profile,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback)
      : SSLErrorHandler(std::move(delegate),
                        web_contents,
                        profile,
                        cert_error,
                        ssl_info,
                        request_url,
                        callback) {}

  using SSLErrorHandler::StartHandlingError;
};

class TestSSLErrorHandlerDelegate : public SSLErrorHandler::Delegate {
 public:
  TestSSLErrorHandlerDelegate(Profile* profile,
                              content::WebContents* web_contents,
                              const net::SSLInfo& ssl_info)
      : profile_(profile),
        captive_portal_checked_(false),
        suggested_url_exists_(false),
        suggested_url_checked_(false),
        ssl_interstitial_shown_(false),
        bad_clock_interstitial_shown_(false),
        captive_portal_interstitial_shown_(false),
        mitm_software_interstitial_shown_(false),
        is_mitm_software_interstitial_enterprise_(false),
        redirected_to_suggested_url_(false),
        is_overridable_error_(true) {}

  void SendCaptivePortalNotification(
      captive_portal::CaptivePortalResult result) {
    CaptivePortalService::Results results;
    results.previous_result = captive_portal::RESULT_INTERNET_CONNECTED;
    results.result = result;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
        content::Source<Profile>(profile_),
        content::Details<CaptivePortalService::Results>(&results));
  }

  void SendSuggestedUrlCheckResult(
      const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
      const GURL& suggested_url) {
    suggested_url_callback_.Run(result, suggested_url);
  }

  int captive_portal_checked() const { return captive_portal_checked_; }
  int ssl_interstitial_shown() const { return ssl_interstitial_shown_; }
  int captive_portal_interstitial_shown() const {
    return captive_portal_interstitial_shown_;
  }
  int mitm_software_interstitial_shown() const {
    return mitm_software_interstitial_shown_;
  }
  bool is_mitm_software_interstitial_enterprise() const {
    return is_mitm_software_interstitial_enterprise_;
  }
  bool bad_clock_interstitial_shown() const {
    return bad_clock_interstitial_shown_;
  }
  bool suggested_url_checked() const { return suggested_url_checked_; }
  bool redirected_to_suggested_url() const {
    return redirected_to_suggested_url_;
  }

  void set_suggested_url_exists() { suggested_url_exists_ = true; }
  void set_non_overridable_error() { is_overridable_error_ = false; }

  void ClearSeenOperations() {
    captive_portal_checked_ = false;
    suggested_url_exists_ = false;
    suggested_url_checked_ = false;
    ssl_interstitial_shown_ = false;
    bad_clock_interstitial_shown_ = false;
    captive_portal_interstitial_shown_ = false;
    mitm_software_interstitial_shown_ = false;
    is_mitm_software_interstitial_enterprise_ = false;
    redirected_to_suggested_url_ = false;
  }

 private:
  void CheckForCaptivePortal() override {
    captive_portal_checked_ = true;
  }

  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override {
    if (!suggested_url_exists_)
      return false;
    *suggested_url = GURL("www.example.com");
    return true;
  }

  void ShowSSLInterstitial() override { ssl_interstitial_shown_ = true; }

  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override {
    bad_clock_interstitial_shown_ = true;
  }

  void ShowCaptivePortalInterstitial(const GURL& landing_url) override {
    captive_portal_interstitial_shown_ = true;
  }

  void ShowMITMSoftwareInterstitial(const std::string& mitm_software_name,
                                    bool is_enterprise_managed) override {
    mitm_software_interstitial_shown_ = true;
    is_mitm_software_interstitial_enterprise_ = is_enterprise_managed;
  }

  void CheckSuggestedUrl(
      const GURL& suggested_url,
      const CommonNameMismatchHandler::CheckUrlCallback& callback) override {
    DCHECK(suggested_url_callback_.is_null());
    suggested_url_checked_ = true;
    suggested_url_callback_ = callback;
  }

  void NavigateToSuggestedURL(const GURL& suggested_url) override {
    redirected_to_suggested_url_ = true;
  }

  bool IsErrorOverridable() const override { return is_overridable_error_; }

  Profile* profile_;
  bool captive_portal_checked_;
  bool suggested_url_exists_;
  bool suggested_url_checked_;
  bool ssl_interstitial_shown_;
  bool bad_clock_interstitial_shown_;
  bool captive_portal_interstitial_shown_;
  bool mitm_software_interstitial_shown_;
  bool is_mitm_software_interstitial_enterprise_;
  bool redirected_to_suggested_url_;
  bool is_overridable_error_;
  CommonNameMismatchHandler::CheckUrlCallback suggested_url_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSSLErrorHandlerDelegate);
};

}  // namespace

// A class to test name mismatch errors. Creates an error handler with a name
// mismatch error.
class SSLErrorHandlerNameMismatchTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerNameMismatchTest() : field_trial_list_(nullptr) {}
  ~SSLErrorHandlerNameMismatchTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ssl_info_.cert = GetCertificate();
    ssl_info_.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));

    delegate_ =
        new TestSSLErrorHandlerDelegate(profile(), web_contents(), ssl_info_);
    error_handler_.reset(new TestSSLErrorHandler(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        profile(), net::MapCertStatusToNetError(ssl_info_.cert_status),
        ssl_info_,
        GURL(),  // request_url
        base::Callback<void(content::CertificateRequestResultType)>()));
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    error_handler_.reset(nullptr);
    SSLErrorHandler::ResetConfigForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 private:
  // Returns a certificate for the test. Virtual to allow derived fixtures to
  // use a certificate with different characteristics.
  virtual scoped_refptr<net::X509Certificate> GetCertificate() {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                   "subjectAltName_www_example_com.pem");
  }

  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  TestSSLErrorHandlerDelegate* delegate_;
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerNameMismatchTest);
};

// A class to test name mismatch errors, where the certificate lacks a
// SubjectAltName. Creates an error handler with a name mismatch error.
class SSLErrorHandlerNameMismatchNoSANTest
    : public SSLErrorHandlerNameMismatchTest {
 public:
  SSLErrorHandlerNameMismatchNoSANTest() {}

 private:
  // Return a certificate that contains no SubjectAltName field.
  scoped_refptr<net::X509Certificate> GetCertificate() override {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  }

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerNameMismatchNoSANTest);
};

// A class to test the captive portal certificate list feature. Creates an error
// handler with a name mismatch error by default. The error handler can be
// recreated by calling ResetErrorHandler() with an appropriate cert status.
class SSLErrorAssistantTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorAssistantTest() : field_trial_list_(nullptr) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ResetErrorHandlerFromFile(kOkayCertName,
                              net::CERT_STATUS_COMMON_NAME_INVALID);
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    error_handler_.reset(nullptr);
    SSLErrorHandler::ResetConfigForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 protected:
  void SetCaptivePortalFeatureEnabled(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitFromCommandLine(
          "CaptivePortalCertificateList" /* enabled */,
          std::string() /* disabled */);
    } else {
      scoped_feature_list_.InitFromCommandLine(
          std::string(), "CaptivePortalCertificateList" /* disabled */);
    }
  }

  void SetMITMSoftwareFeatureEnabled(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitFromCommandLine(
          "MITMSoftwareInterstitial" /* enabled */,
          std::string() /* disabled */);
    } else {
      scoped_feature_list_.InitFromCommandLine(
          std::string(), "MITMSoftwareInterstitial" /* disabled */);
    }
  }

  void ResetErrorHandlerFromString(const std::string& cert_data,
                                   net::CertStatus cert_status) {
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert_data.data(), cert_data.size(),
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(certs.empty());
    ResetErrorHandler(certs[0], cert_status);
  }

  void ResetErrorHandlerFromFile(const std::string& cert_name,
                                 net::CertStatus cert_status) {
    ResetErrorHandler(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert_name),
        cert_status);
  }

  // Set up an error assistant proto with mock captive portal hash data and
  // begin handling the certificate error.
  void RunCaptivePortalTest() {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

    auto config_proto =
        base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        "sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        ssl_info().public_key_hashes[0].ToString());
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

    error_handler()->StartHandlingError();
  }

  void TestNoCaptivePortalInterstitial() {
    base::HistogramTester histograms;

    RunCaptivePortalTest();

    // Timer should start for captive portal detection.
    EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->captive_portal_checked());
    EXPECT_FALSE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    // Check that the histogram for the captive portal cert was recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }

  // Set up a mock SSL Error Assistant config with regexes that match the
  // outdated antivirus and misconfigured firewall certificate.
  void InitMITMSoftwareList() {
    auto config_proto =
        base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    chrome_browser_ssl::MITMSoftware* filter1 =
        config_proto->add_mitm_software();
    filter1->set_name("Outdated Antivirus");
    filter1->set_regex("Outdated Antivirus");

    chrome_browser_ssl::MITMSoftware* filter2 =
        config_proto->add_mitm_software();
    filter2->set_name("Misconfigured Firewall");
    filter2->set_regex("Misconfigured Firewall_[A-Z0-9]+");
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  }

  // Calls RunMITMSoftwareTest() to set up an error assistant proto with mock
  // MITM software strings and start handling the SSL error. Check that the
  // generic interstitial was not shown, and the MITM software interstitial
  // was. Check the UMA histograms.
  void TestMITMSoftwareInterstitial() {
    base::HistogramTester histograms;

    InitMITMSoftwareList();
    error_handler()->StartHandlingError();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(delegate()->ssl_interstitial_shown());
    EXPECT_TRUE(delegate()->mitm_software_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 1);
  }

  // Calls RunMITMSoftwareTest() to set up an error assistant proto with mock
  // MITM software strings and start handling the SSL error. Check that the
  // MITM software interstitial is not shown, and a nonoverridable generic SSL
  // interstitial is shown in its place. Check UMA histograms.
  void TestNoMITMSoftwareInterstitial() {
    base::HistogramTester histograms;

    InitMITMSoftwareList();
    error_handler()->StartHandlingError();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->mitm_software_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 0);
  }

 private:
  void ResetErrorHandler(scoped_refptr<net::X509Certificate> cert,
                         net::CertStatus cert_status) {
    ssl_info_.Reset();
    ssl_info_.cert = cert;
    ssl_info_.cert_status = cert_status;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));

    delegate_ =
        new TestSSLErrorHandlerDelegate(profile(), web_contents(), ssl_info_);
    error_handler_.reset(new TestSSLErrorHandler(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        profile(), net::MapCertStatusToNetError(ssl_info_.cert_status),
        ssl_info_,
        GURL(),  // request_url
        base::Callback<void(content::CertificateRequestResultType)>()));
  }

  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  TestSSLErrorHandlerDelegate* delegate_;
  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorAssistantTest);
};

class SSLErrorHandlerDateInvalidTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerDateInvalidTest()
      : field_trial_test_(new network_time::FieldTrialTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock),
        test_server_(new net::EmbeddedTestServer) {
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
    network_time::NetworkTimeTracker::RegisterPrefs(pref_service_.registry());
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();

    field_trial_test()->SetNetworkQueriesWithVariationsService(
        false, 0.0,
        network_time::NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);
    tracker_.reset(new network_time::NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_,
        new net::TestURLRequestContextGetter(
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::IO))));

    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::TimeDelta::FromDays(111));
    tick_clock_->Advance(base::TimeDelta::FromDays(222));

    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ssl_info_.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ssl_info_.cert_status = net::CERT_STATUS_DATE_INVALID;

    delegate_ =
        new TestSSLErrorHandlerDelegate(profile(), web_contents(), ssl_info_);
    error_handler_.reset(new TestSSLErrorHandler(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        profile(), net::MapCertStatusToNetError(ssl_info_.cert_status),
        ssl_info_,
        GURL(),  // request_url
        base::Callback<void(content::CertificateRequestResultType)>()));
    error_handler_->SetNetworkTimeTrackerForTesting(tracker_.get());

    // Fix flakiness in case system time is off and triggers a bad clock
    // interstitial. https://crbug.com/666821#c50
    ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  }

  void TearDown() override {
    if (error_handler()) {
      EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
      error_handler_.reset(nullptr);
    }
    SSLErrorHandler::ResetConfigForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  network_time::FieldTrialTest* field_trial_test() {
    return field_trial_test_.get();
  }

  network_time::NetworkTimeTracker* tracker() { return tracker_.get(); }

  net::EmbeddedTestServer* test_server() { return test_server_.get(); }

  void ClearErrorHandler() { error_handler_.reset(nullptr); }

 private:
  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  TestSSLErrorHandlerDelegate* delegate_;

  std::unique_ptr<network_time::FieldTrialTest> field_trial_test_;
  base::SimpleTestClock* clock_;
  base::SimpleTestTickClock* tick_clock_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<network_time::NetworkTimeTracker> tracker_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerDateInvalidTest);
};

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpired) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  delegate()->ClearSeenOperations();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowCustomInterstitialOnCaptivePortalResult) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  // Fake a captive portal result.
  delegate()->ClearSeenOperations();
  delegate()->SendCaptivePortalNotification(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnNoCaptivePortalResult) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  // Fake a "connected to internet" result for the captive portal check.
  // This should immediately trigger an SSL interstitial without waiting for
  // the timer to expire.
  delegate()->ClearSeenOperations();
  delegate()->SendCaptivePortalNotification(
      captive_portal::RESULT_INTERNET_CONNECTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckSuggestedUrlIfNoSuggestedUrl) {
  base::HistogramTester histograms;
  error_handler()->StartHandlingError();

  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckCaptivePortalIfSuggestedUrlExists) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  // Note that the suggested URL check is never completed, so there is no entry
  // for WWW_MISMATCH_URL_AVAILABLE or WWW_MISMATCH_URL_NOT_AVAILABLE.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotHandleNameMismatchOnNonOverridableError) {
  base::HistogramTester histograms;
  delegate()->set_non_overridable_error();
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(delegate()->suggested_url_checked());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
}

#else  // #if !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnCaptivePortalDetectionDisabled) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpiredWhenSuggestedUrlExists) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  // Note that the suggested URL check is never completed, so there is no entry
  // for WWW_MISMATCH_URL_AVAILABLE or WWW_MISMATCH_URL_NOT_AVAILABLE.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldRedirectOnSuggestedUrlCheckResult) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());
  // Fake a valid suggested URL check result.
  // The URL returned by |SuggestedUrlCheckResult| can be different from
  // |suggested_url|, if there is a redirect.
  delegate()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_AVAILABLE,
      GURL("https://random.example.com"));

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_URL_AVAILABLE, 1);
}

// No suggestions should be requested if certificate lacks a SubjectAltName.
TEST_F(SSLErrorHandlerNameMismatchNoSANTest,
       SSLCommonNameMismatchHandlingRequiresSubjectAltName) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(delegate()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 0);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnInvalidUrlCheckResult) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());
  // Fake an Invalid Suggested URL Check result.
  delegate()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_NOT_AVAILABLE,
      GURL());

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 4);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_URL_NOT_AVAILABLE,
                               1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerDateInvalidTest, TimeQueryStarted) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. A bad clock interstitial
  // should be shown.
  test_server()->RegisterRequestHandler(
      base::Bind(&network_time::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  tracker()->WaitForFetchForTesting(123123123);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->bad_clock_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  // Check that the histogram for the delay was recorded.
  histograms.ExpectTotalCount(kCertDateErrorHistogram, 1);
}

// Tests that an SSL interstitial is shown if the accuracy of the system
// clock can't be determined because network time is unavailable.
TEST_F(SSLErrorHandlerDateInvalidTest, NoTimeQueries) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Handle the error without enabling time queries. A bad clock interstitial
  // should not be shown.
  error_handler()->StartHandlingError();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->bad_clock_interstitial_shown());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  // Check that the histogram for the delay was recorded.
  histograms.ExpectTotalCount(kCertDateErrorHistogram, 1);
}

// Tests that an SSL interstitial is shown if determing the accuracy of
// the system clock times out (e.g. because a network time query hangs).
TEST_F(SSLErrorHandlerDateInvalidTest, TimeQueryHangs) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. Because the
  // network time cannot be determined before the timer elapses, an SSL
  // interstitial should be shown.
  base::RunLoop wait_for_time_query_loop;
  test_server()->RegisterRequestHandler(
      base::Bind(&WaitForRequest, wait_for_time_query_loop.QuitClosure()));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  wait_for_time_query_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate()->bad_clock_interstitial_shown());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());

  // Check that the histogram for the delay was recorded.
  histograms.ExpectTotalCount(kCertDateErrorHistogram, 1);

  // Clear the error handler to test that, when the request completes,
  // it doesn't try to call a callback on a deleted SSLErrorHandler.
  ClearErrorHandler();

  // Shut down the server to cancel the pending request.
  ASSERT_TRUE(test_server()->ShutdownAndWaitUntilComplete());
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

// Tests that a certificate marked as a known captive portal certificate causes
// the captive portal interstitial to be shown.
TEST_F(SSLErrorAssistantTest, CaptivePortal_FeatureEnabled) {
  SetCaptivePortalFeatureEnabled(true);

  base::HistogramTester histograms;

  RunCaptivePortalTest();

  // Timer shouldn't start for a known captive portal certificate.
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  // A buggy SSL error handler might have incorrectly started the timer. Run
  // to completion to ensure the timer is expired.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
}

// Tests that a certificate marked as a known captive portal certificate does
// not cause the captive portal interstitial to be shown, if the feature is
// disabled.
TEST_F(SSLErrorAssistantTest, CaptivePortal_FeatureDisabled) {
  SetCaptivePortalFeatureEnabled(false);

  // Default error for SSLErrorHandlerNameMismatchTest tests is name mismatch.
  TestNoCaptivePortalInterstitial();
}

// Tests that an error other than name mismatch does not cause a captive portal
// interstitial to be shown, even if the certificate is marked as a known
// captive portal certificate.
TEST_F(SSLErrorAssistantTest,
       CaptivePortal_AuthorityInvalidError_NoInterstitial) {
  SetCaptivePortalFeatureEnabled(true);

  ResetErrorHandlerFromFile(kOkayCertName, net::CERT_STATUS_AUTHORITY_INVALID);
  TestNoCaptivePortalInterstitial();
}

// Tests that an authority invalid error in addition to name mismatch error does
// not cause a captive portal interstitial to be shown, even if the certificate
// is marked as a known captive portal certificate. The resulting error is
// authority-invalid.
TEST_F(SSLErrorAssistantTest, CaptivePortal_TwoErrors_NoInterstitial) {
  SetCaptivePortalFeatureEnabled(true);

  const net::CertStatus cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_AUTHORITY_INVALID;
  // Sanity check that AUTHORITY_INVALID is seen as the net error.
  ASSERT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            net::MapCertStatusToNetError(cert_status));
  ResetErrorHandlerFromFile(kOkayCertName, cert_status);
  TestNoCaptivePortalInterstitial();
}

// Tests that another error in addition to name mismatch error does not cause a
// captive portal interstitial to be shown, even if the certificate is marked as
// a known captive portal certificate. Similar to
// NameMismatchAndAuthorityInvalid, except the resulting error is name mismatch.
TEST_F(SSLErrorAssistantTest,
       CaptivePortal_TwoErrorsIncludingNameMismatch_NoInterstitial) {
  SetCaptivePortalFeatureEnabled(true);

  const net::CertStatus cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_WEAK_KEY;
  // Sanity check that COMMON_NAME_INVALID is seen as the net error, since the
  // test is designed to verify that SSLErrorHandler notices other errors in the
  // CertStatus even when COMMON_NAME_INVALID is the net error.
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(cert_status));
  ResetErrorHandlerFromFile(kOkayCertName, cert_status);
  TestNoCaptivePortalInterstitial();
}

#else

TEST_F(SSLErrorAssistantTest, CaptivePortal_DisabledByBuild) {
  SetCaptivePortalFeatureEnabled(true);

  // Default error for SSLErrorHandlerNameMismatchTest tests is name mismatch,
  // but the feature is disabled by build so a generic SSL interstitial will be
  // displayed.
  base::HistogramTester histograms;

  RunCaptivePortalTest();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

#if !defined(OS_IOS)

TEST_F(SSLErrorAssistantTest, MITMSoftware_OutdatedAntivirusCertificate) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  delegate()->set_non_overridable_error();

  TestMITMSoftwareInterstitial();
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_MisconfiguredFirewallCertificate) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  delegate()->set_non_overridable_error();

  TestMITMSoftwareInterstitial();
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_EnterpriseManaged) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(true);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());
  delegate()->set_non_overridable_error();

  TestMITMSoftwareInterstitial();

  EXPECT_TRUE(delegate()->is_mitm_software_interstitial_enterprise());
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_NotEnterpriseManaged) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(false);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());
  delegate()->set_non_overridable_error();

  TestMITMSoftwareInterstitial();

  EXPECT_FALSE(delegate()->is_mitm_software_interstitial_enterprise());
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_FeatureDisabled) {
  SetMITMSoftwareFeatureEnabled(false);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  delegate()->set_non_overridable_error();

  TestNoMITMSoftwareInterstitial();
}

TEST_F(SSLErrorAssistantTest,
       MITMSoftware_NonMatchingCertificate_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromFile(kOkayCertName, net::CERT_STATUS_AUTHORITY_INVALID);
  delegate()->set_non_overridable_error();

  TestNoMITMSoftwareInterstitial();
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_WrongError_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_COMMON_NAME_INVALID);
  delegate()->set_non_overridable_error();

  TestNoMITMSoftwareInterstitial();
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_TwoErrors_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID |
                                  net::CERT_STATUS_COMMON_NAME_INVALID);
  delegate()->set_non_overridable_error();

  TestNoMITMSoftwareInterstitial();
}

TEST_F(SSLErrorAssistantTest, MITMSoftware_Overridable_NoInterstitial) {
  base::HistogramTester histograms;

  SetMITMSoftwareFeatureEnabled(true);
  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  // Don't use the TestNoMITMSoftwareInterstitial helper here because it
  // checks the histograms for a nonoverridable SSL error.
  InitMITMSoftwareList();
  error_handler()->StartHandlingError();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->mitm_software_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

#else

TEST_F(SSLErrorAssistantTest, MITMSoftware_DisabledByBuild_NoInterstitial) {
  SetMITMSoftwareFeatureEnabled(true);

  ResetErrorHandlerFromString(kOutdatedAntivirusCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  delegate()->set_non_overridable_error();

  TestNoMITMSoftwareInterstitial();
}

#endif  // #if !defined(OS_IOS)
