// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/captive_portal_blocking_page.h"

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
// Partial text in the captive portal interstitial's main paragraph when the
// login domain isn't displayed.
const char kGenericLoginURLText[] = "its login page";
const char kBrokenSSL[] = "https://broken.ssl";
const char kWiFiSSID[] = "WiFiSSID";

// Returns true if the interstitial contains |text| in its body.
bool IsInterstitialDisplayingText(content::InterstitialPage* interstitial,
                                  const std::string& text) {
  // It's valid for |text| to contain "\'", but simply look for "'" instead
  // since this function is used for searching host names and a predefined
  // string.
  DCHECK(text.find("\'") == std::string::npos);
  std::string command = base::StringPrintf(
      "var hasText = document.body.textContent.indexOf('%s') >= 0;"
      "window.domAutomationController.send(hasText ? 1 : 0);",
      text.c_str());
  int result = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      interstitial->GetMainFrame(), command, &result));
  return result == 1;
}

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

class FakeConnectionInfoDelegate : public CaptivePortalBlockingPage::Delegate {
 public:
  FakeConnectionInfoDelegate(bool is_wifi_connection, std::string wifi_ssid)
      : is_wifi_connection_(is_wifi_connection), wifi_ssid_(wifi_ssid) {}
  ~FakeConnectionInfoDelegate() override {}

  bool IsWifiConnection() const override { return is_wifi_connection_; }
  std::string GetWiFiSSID() const override { return wifi_ssid_; }

 private:
  const bool is_wifi_connection_;
  const std::string wifi_ssid_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionInfoDelegate);
};

class CaptivePortalBlockingPageTest : public InProcessBrowserTest {
 public:
  CaptivePortalBlockingPageTest() {}

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url,
                        const std::string& expected_login_hostname);

  void TestInterstitial(bool is_wifi_connection,
                        const std::string& wifi_ssid,
                        const GURL& login_url,
                        ExpectWiFi expect_wifi,
                        ExpectWiFiSSID expect_wifi_ssid,
                        ExpectLoginURL expect_login_url);

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
    const std::string& expected_login_hostname) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  // Delegate is owned by the blocking page.
  FakeConnectionInfoDelegate* delegate =
      new FakeConnectionInfoDelegate(is_wifi_connection, wifi_ssid);
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      contents, GURL(kBrokenSSL), login_url, base::Callback<void(bool)>());
  blocking_page->SetDelegateForTesting(delegate);
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
  TestInterstitial(is_wifi_connection, wifi_ssid, login_url,
                   expect_wifi, expect_wifi_ssid, expect_login_url,
                   login_url.host());
}

// If the connection is not a Wi-Fi connection, the wired network version of the
// captive portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       WiredNetwork_LoginURL) {
  TestInterstitial(false, "", GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_NO, EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
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
  TestInterstitial(false, "", kLandingUrl, EXPECT_WIFI_NO, EXPECT_WIFI_SSID_NO,
                   EXPECT_LOGIN_URL_NO);
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
  TestInterstitial(true, "", GURL("http://captive.portal/landing_url"),
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_YES);
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
  TestInterstitial(true, "", kLandingUrl,
                   EXPECT_WIFI_YES, EXPECT_WIFI_SSID_NO, EXPECT_LOGIN_URL_NO);
}

class CaptivePortalBlockingPageIDNTest : public CaptivePortalBlockingPageTest {
public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Clear AcceptLanguages to force punycode decoding.
    browser()->profile()->GetPrefs()->SetString(prefs::kAcceptLanguages,
                                                std::string());
  }
};

// Same as CaptivePortalBlockingPageTest.WiredNetwork_LoginURL, except the login
// domain is an IDN.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageIDNTest,
                       ShowLoginIDNIfPortalRedirectsDetectionURL) {
  const char kHostname[] =
      "xn--d1abbgf6aiiy.xn--p1ai";
  const char kHostnameJSUnicode[] =
      "\\u043f\\u0440\\u0435\\u0437\\u0438\\u0434\\u0435\\u043d\\u0442."
      "\\u0440\\u0444";
  std::string landing_url_spec =
      base::StringPrintf("http://%s/landing_url", kHostname);
  GURL landing_url(landing_url_spec);

  TestInterstitial(false, "", landing_url, EXPECT_WIFI_NO, EXPECT_WIFI_SSID_NO,
                   EXPECT_LOGIN_URL_YES, kHostnameJSUnicode);
}
