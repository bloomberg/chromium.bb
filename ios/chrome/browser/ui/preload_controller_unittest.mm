// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/ios/device_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/preload_controller.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PreloadController (ExposedForTesting)
- (BOOL)shouldPreloadURL:(const GURL&)url;
- (BOOL)isPrerenderingEnabled;
- (BOOL)isPrefetchingEnabled;
- (const GURL)urlToPrefetchURL:(const GURL&)url;
- (BOOL)hasPrefetchedURL:(const GURL&)url;
@end

namespace {

// Override NetworkChangeNotifier to simulate connection type changes for tests.
class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier()
      : net::NetworkChangeNotifier(),
        connection_type_to_return_(
            net::NetworkChangeNotifier::CONNECTION_UNKNOWN) {}

  // Simulates a change of the connection type to |type|. This will notify any
  // objects that are NetworkChangeNotifiers.
  void SimulateNetworkConnectionChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    connection_type_to_return_ = type;
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
    base::RunLoop().RunUntilIdle();
  }

 private:
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_to_return_;
  }

  // The currently simulated network connection type. If this is set to
  // CONNECTION_NONE, then NetworkChangeNotifier::IsOffline will return true.
  net::NetworkChangeNotifier::ConnectionType connection_type_to_return_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

class PreloadControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    // Set up a NetworkChangeNotifier so that the test can simulate Wi-Fi vs.
    // cellular connection.
    network_change_notifier_.reset(new TestNetworkChangeNotifier);

    test_url_fetcher_factory_.reset(new net::TestURLFetcherFactory());

    controller_ = [[PreloadController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  };

  // Set the "Preload webpages" setting to "Always".
  void PreloadWebpagesAlways() {
    chrome_browser_state_->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionEnabled, YES);
    chrome_browser_state_->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionWifiOnly, NO);
  }

  // Set the "Preload webpages" setting to "Only on Wi-Fi".
  void PreloadWebpagesWiFiOnly() {
    chrome_browser_state_->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionEnabled, YES);
    chrome_browser_state_->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionWifiOnly, YES);
  }

  // Set the "Preload webpages" setting to "Never".
  void PreloadWebpagesNever() {
    chrome_browser_state_->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionEnabled, NO);
  }

  void SimulateWiFiConnection() {
    network_change_notifier_->SimulateNetworkConnectionChange(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

  void SimulateCellularConnection() {
    network_change_notifier_->SimulateNetworkConnectionChange(
        net::NetworkChangeNotifier::CONNECTION_3G);
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<net::TestURLFetcherFactory> test_url_fetcher_factory_;
  PreloadController* controller_;
};

// Tests that the preload controller does not try to preload non-web urls.
TEST_F(PreloadControllerTest, ShouldPreloadURL) {
  EXPECT_TRUE([controller_ shouldPreloadURL:GURL("http://www.google.com/")]);
  EXPECT_TRUE([controller_ shouldPreloadURL:GURL("https://www.google.com/")]);

  EXPECT_FALSE([controller_ shouldPreloadURL:GURL()]);
  EXPECT_FALSE([controller_ shouldPreloadURL:GURL("chrome://newtab")]);
  EXPECT_FALSE([controller_ shouldPreloadURL:GURL("about:flags")]);
}

TEST_F(PreloadControllerTest, TestIsPrerenderingEnabled_preloadAlways) {
  // With the "Preload Webpages" setting set to "Always", prerendering is
  // enabled regardless of network type.
  PreloadWebpagesAlways();

  SimulateWiFiConnection();
  EXPECT_TRUE([controller_ isPrerenderingEnabled] ||
              ios::device_util::IsSingleCoreDevice() ||
              !ios::device_util::RamIsAtLeast512Mb());

  SimulateCellularConnection();
  EXPECT_TRUE([controller_ isPrerenderingEnabled] ||
              ios::device_util::IsSingleCoreDevice() ||
              !ios::device_util::RamIsAtLeast512Mb());
}

TEST_F(PreloadControllerTest, TestIsPrerenderingEnabled_preloadWiFiOnly) {
  // With the Chrome "Preload Webpages" setting set to "Only on Wi-Fi",
  // prerendering is enabled only on WiFi.
  PreloadWebpagesWiFiOnly();

  SimulateWiFiConnection();
  EXPECT_TRUE([controller_ isPrerenderingEnabled] ||
              ios::device_util::IsSingleCoreDevice() ||
              !ios::device_util::RamIsAtLeast512Mb());

  SimulateCellularConnection();
  EXPECT_FALSE([controller_ isPrerenderingEnabled]);
}

TEST_F(PreloadControllerTest, TestIsPrerenderingEnabled_preloadNever) {
  // With the Chrome "Preload Webpages" setting set to "Never", prerendering
  // is never enabled, regardless of the network type.
  PreloadWebpagesNever();

  SimulateWiFiConnection();
  EXPECT_FALSE([controller_ isPrerenderingEnabled]);

  SimulateCellularConnection();
  EXPECT_FALSE([controller_ isPrerenderingEnabled]);
}

TEST_F(PreloadControllerTest, TestIsPrefetchingEnabled_preloadAlways) {
  // With the "Preload Webpages" setting set to "Always", prefetching is
  // always enabled.
  PreloadWebpagesAlways();

  SimulateWiFiConnection();
  EXPECT_TRUE([controller_ isPrefetchingEnabled]);

  SimulateCellularConnection();
  EXPECT_TRUE([controller_ isPrefetchingEnabled]);
}

TEST_F(PreloadControllerTest, TestIsPrefetchingEnabled_preloadWiFiOnly) {
  // With the Chrome "Preload Webpages" setting set to "Only on Wi-Fi",
  // prefetching is enabled only on WiFi.
  PreloadWebpagesWiFiOnly();

  SimulateWiFiConnection();
  EXPECT_TRUE([controller_ isPrefetchingEnabled]);

  SimulateCellularConnection();
  EXPECT_FALSE([controller_ isPrefetchingEnabled]);
}

TEST_F(PreloadControllerTest, TestIsPrefetchingEnabled_preloadNever) {
  // With the Chrome "Preload Webpages" setting set to "Never", prefetching
  // is never enabled, regardless of WiFi state.
  PreloadWebpagesNever();

  SimulateWiFiConnection();
  EXPECT_FALSE([controller_ isPrefetchingEnabled]);

  SimulateCellularConnection();
  EXPECT_FALSE([controller_ isPrefetchingEnabled]);
}

TEST_F(PreloadControllerTest, TestPrefetchURL_transformURL) {
  PreloadWebpagesAlways();

  GURL original("http://www.google.com/search?q=foo");
  GURL expected("http://www.google.com/search?q=foo&pf=i");
  [controller_ prefetchURL:original
                transition:ui::PAGE_TRANSITION_FROM_ADDRESS_BAR];

  net::TestURLFetcher* url_fetcher = nil;
  url_fetcher =
      test_url_fetcher_factory_->GetFetcherByID(kPreloadControllerURLFetcherID);

  EXPECT_TRUE(url_fetcher);
  GURL actual = url_fetcher->GetOriginalURL();
  EXPECT_EQ(expected, actual);
}

TEST_F(PreloadControllerTest, TestUrlToPrefetchURL_noParams) {
  GURL original("http://www.google.com/search");
  GURL expected("http://www.google.com/search?pf=i");
  GURL actual = [controller_ urlToPrefetchURL:original];
  EXPECT_EQ(expected, actual);
}

TEST_F(PreloadControllerTest, TestUrlToPrefetchURL_params) {
  std::string urlString =
      std::string("http://www.google.com/search")
          .append("?q=legoland&oq=legol&aqs=chrome.0.0j69i57j0j5")
          .append("&sourceid=chrome-mobile&ie=UTF-8&hl=en-US");
  GURL original(urlString);
  GURL expected(urlString + "&pf=i");
  GURL actual = [controller_ urlToPrefetchURL:original];
  EXPECT_EQ(expected, actual);
}

TEST_F(PreloadControllerTest, TestHasPrefetchedURL) {
  PreloadWebpagesAlways();

  GURL first("http://www.google.com/search?q=first");
  GURL second("http://www.google.com/search?q=second");
  GURL bogus("http://www.google.com/search?q=bogus");

  EXPECT_FALSE([controller_ hasPrefetchedURL:first]);
  EXPECT_FALSE([controller_ hasPrefetchedURL:second]);
  EXPECT_FALSE([controller_ hasPrefetchedURL:bogus]);

  // Prefetch |first| and verify it's the only one that returns true.
  [controller_ prefetchURL:first
                transition:ui::PAGE_TRANSITION_FROM_ADDRESS_BAR];
  EXPECT_TRUE([controller_ hasPrefetchedURL:first]);
  EXPECT_FALSE([controller_ hasPrefetchedURL:second]);
  EXPECT_FALSE([controller_ hasPrefetchedURL:bogus]);

  // Prefetch |second| and verify it's the only one that returns true.
  [controller_ prefetchURL:second
                transition:ui::PAGE_TRANSITION_FROM_ADDRESS_BAR];
  EXPECT_FALSE([controller_ hasPrefetchedURL:first]);
  EXPECT_TRUE([controller_ hasPrefetchedURL:second]);
  EXPECT_FALSE([controller_ hasPrefetchedURL:bogus]);
}

}  // anonymous namespace
