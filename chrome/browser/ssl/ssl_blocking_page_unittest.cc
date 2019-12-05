// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace OnSecurityInterstitialShown =
    extensions::api::safe_browsing_private::OnSecurityInterstitialShown;
namespace OnSecurityInterstitialProceeded =
    extensions::api::safe_browsing_private::OnSecurityInterstitialProceeded;

class SSLBlockingPageTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLBlockingPageTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_event_router_ =
        extensions::CreateAndUseTestEventRouter(browser_context());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            browser_context(),
            base::BindRepeating(
                &safe_browsing::BuildSafeBrowsingPrivateEventRouter));
  }

  extensions::TestEventRouter* test_event_router() {
    return test_event_router_;
  }

 private:
  extensions::TestEventRouter* test_event_router_;
};

// TODO(1026557): This test currently fails, and it looks like it also tests
// the pre-committed interstitials flow. It needs to be updated, made to pass,
// and potentially componentized to live next to SSLBlockingPage.
TEST_F(SSLBlockingPageTest,
       DISABLED_VerifySecurityInterstitialExtensionEvents) {
  safe_browsing::TestExtensionEventObserver observer(test_event_router());

  // Sets up elements needed for a SSL blcocking page.
  GURL request_url("https://error.example.test");
  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;

  // Simulates the showing of a SSL blocking page.
  SSLBlockingPage* blocking_page =
      ChromeSecurityBlockingPageFactory::CreateSSLPage(
          web_contents(), net::ERR_CERT_DATE_INVALID, ssl_info, request_url,
          /*options_mask=*/0, base::Time::NowFromSystemTime(),
          /*support_url=*/GURL(),
          /*ssl_cert_reporter=*/nullptr);
  blocking_page->DontCreateViewForTesting();
  blocking_page->Show();

  // Verifies that security interstitial shown event is observed.
  observer.VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialShown::kEventName, request_url, "SSL_ERROR", "",
      net::ERR_CERT_DATE_INVALID);

  // Simulates a proceed action.
  blocking_page->CommandReceived(
      base::NumberToString(security_interstitials::CMD_PROCEED));

  // Verifies that security interstitial proceeded event is observed.
  observer.VerifyLatestSecurityInterstitialEvent(
      OnSecurityInterstitialProceeded::kEventName, request_url, "SSL_ERROR", "",
      net::ERR_CERT_DATE_INVALID);
}
