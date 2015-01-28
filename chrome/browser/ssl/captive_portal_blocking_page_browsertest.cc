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

// Partial text in the captive portal interstitial's main paragraph when the
// login domain isn't displayed.
const char kGenericLoginURLText[] = "its login page";
const char kBrokenSSL[] = "https://broken.ssl";

// Returns true if the interstitial contains |text| in its body.
bool IsInterstitialDisplayingText(content::InterstitialPage* interstitial,
                                  const std::string& text) {
  // It's valid for |text| to contain "\'", but simply look for "'" instead
  // since this function is used for searching host names and a predefined
  // string.
  DCHECK(text.find("\'") == std::string::npos);
  std::string command = base::StringPrintf(
      "var hasText = document.body.textContent.indexOf('%s') >= 0;"
      "window.domAutomationController.send(hasText);",
      text.c_str());
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      interstitial->GetMainFrame(), command, &result));
  return result;
}

class CaptivePortalBlockingPageTest : public InProcessBrowserTest {
 public:
  CaptivePortalBlockingPageTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBlockingPageTest);
};

// If the connection is not a Wi-Fi connection, the wired network version of the
// captive portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       ShowWiredNetworkInterstitial) {
  const GURL kLandingUrl("http://captive.portal/landing_url");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      contents, GURL(kBrokenSSL), kLandingUrl, base::Callback<void(bool)>());
  blocking_page->SetWiFiConnectionForTesting(false);
  blocking_page->Show();

  WaitForInterstitialAttach(contents);
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));
  EXPECT_FALSE(
      IsInterstitialDisplayingText(contents->GetInterstitialPage(), "Wi-Fi"));
}

// If the connection is a Wi-Fi connection, the Wi-Fi version of the captive
// portal interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       ShowWiFiInterstitial) {
  const GURL kLandingUrl("http://captive.portal/landing_url");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      contents, GURL(kBrokenSSL), kLandingUrl, base::Callback<void(bool)>());
  blocking_page->SetWiFiConnectionForTesting(true);
  blocking_page->Show();

  WaitForInterstitialAttach(contents);
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));
  EXPECT_TRUE(
      IsInterstitialDisplayingText(contents->GetInterstitialPage(), "Wi-Fi"));
}

// The captive portal interstitial should show the login url if the login url
// is different than the captive portal ping url (i.e. the portal intercepts
// requests via HTTP redirects).
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       ShowLoginDomainIfPortalRedirectsDetectionURL) {
  const GURL kLandingUrl("http://captive.portal/landing_url");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      contents, GURL(kBrokenSSL), kLandingUrl, base::Callback<void(bool)>());
  blocking_page->SetWiFiConnectionForTesting(false);
  blocking_page->Show();

  WaitForInterstitialAttach(contents);
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));

  EXPECT_TRUE(IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                           kLandingUrl.host()));
  EXPECT_FALSE(IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                            kGenericLoginURLText));
}

// The captive portal interstitial should show a generic text if the login url
// is the same as the captive portal ping url (i.e. the portal intercepts
// requests without using HTTP redirects).
IN_PROC_BROWSER_TEST_F(CaptivePortalBlockingPageTest,
                       DontShowLoginDomainIfPortalDoesntRedirectDetectionURL) {
  const GURL kLandingUrl(captive_portal::CaptivePortalDetector::kDefaultURL);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      contents, GURL(kBrokenSSL), kLandingUrl, base::Callback<void(bool)>());
  blocking_page->SetWiFiConnectionForTesting(false);
  blocking_page->Show();

  WaitForInterstitialAttach(contents);
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));

  EXPECT_FALSE(IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                            kLandingUrl.host()));
  EXPECT_TRUE(IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                           kGenericLoginURLText));
}

class CaptivePortalBlockingPageIDNTest : public InProcessBrowserTest {
public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Clear AcceptLanguages to force punycode decoding.
    browser()->profile()->GetPrefs()->SetString(prefs::kAcceptLanguages,
                                                std::string());
  }
};

// Same as ShowLoginDomainIfPortalRedirectsDetectionURL, except the login
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

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  // Blocking page is owned by the interstitial.
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      contents, GURL(kBrokenSSL), landing_url, base::Callback<void(bool)>());
  blocking_page->SetWiFiConnectionForTesting(false);
  blocking_page->Show();

  WaitForInterstitialAttach(contents);
  EXPECT_TRUE(
      WaitForRenderFrameReady(contents->GetInterstitialPage()->GetMainFrame()));

  EXPECT_TRUE(IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                           kHostnameJSUnicode));
  EXPECT_FALSE(IsInterstitialDisplayingText(contents->GetInterstitialPage(),
                                            kGenericLoginURLText));
}
