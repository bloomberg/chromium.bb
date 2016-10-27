// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_external_protocol_dialog.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kChromePackageName[] = "org.chromium.arc.intent_helper";

// Creates and returns a new IntentHandlerInfo object.
mojom::IntentHandlerInfoPtr Create(const std::string& name,
                                   const std::string& package_name,
                                   bool is_preferred,
                                   const GURL& fallback_url) {
  mojom::IntentHandlerInfoPtr ptr = mojom::IntentHandlerInfo::New();
  ptr->name = name;
  ptr->package_name = package_name;
  ptr->is_preferred = is_preferred;
  if (!fallback_url.is_empty())
    ptr->fallback_url = fallback_url.spec();
  return ptr;
}

}  // namespace

// Tests that when no apps are returned from ARC, GetAction returns
// SHOW_CHROME_OS_DIALOG.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithNoApp) {
  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::SHOW_CHROME_OS_DIALOG,
            GetActionForTesting(GURL("external-protocol:foo"), handlers,
                                handlers.size(), &url_and_package));
}

// Tests that when one app is passed to GetAction but the user hasn't selected
// it, the function returns ASK_USER.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneApp) {
  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("package", "com.google.package.name", false, GURL()));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::ASK_USER,
            GetActionForTesting(GURL("external-protocol:foo"), handlers,
                                no_selection, &url_and_package));
}

// Tests that when one preferred app is passed to GetAction, the function
// returns HANDLE_URL_IN_ARC even if the user hasn't selected the app.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOnePreferredApp) {
  const GURL external_url("external-protocol:foo");
  const std::string package_name("com.google.package.name");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("package", package_name, true, GURL()));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(external_url, handlers, no_selection,
                                &url_and_package));
  EXPECT_EQ(external_url, url_and_package.first);
  EXPECT_EQ(package_name, url_and_package.second);
}

// Tests that when one app is passed to GetAction, the user has already selected
// it, the function returns HANDLE_URL_IN_ARC.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneAppSelected) {
  const GURL external_url("external-protocol:foo");
  const std::string package_name("com.google.package.name");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("package", package_name, false, GURL()));

  constexpr size_t kSelection = 0;
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(external_url, handlers, kSelection,
                                &url_and_package));
  EXPECT_EQ(external_url, url_and_package.first);
  EXPECT_EQ(package_name, url_and_package.second);
}

// Tests the same as TestGetActionWithOnePreferredApp but with two apps.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOnePreferredAppAndOneOther) {
  const GURL external_url("external-protocol:foo");
  const std::string package_name("com.google.package2.name");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("package", "com.google.package.name", false, GURL()));
  handlers.push_back(Create("package2", package_name, true, GURL()));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(external_url, handlers, no_selection,
                                &url_and_package));
  EXPECT_EQ(external_url, url_and_package.first);
  EXPECT_EQ(package_name, url_and_package.second);
}

// Tests that HANDLE_URL_IN_ARC is returned for geo: URL. The URL is special in
// that intent_helper (i.e. the Chrome proxy) can handle it but Chrome cannot.
// We have to send such a URL to intent_helper to let the helper rewrite the
// URL to https://maps.google.com/?latlon=xxx which Chrome can handle.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithGeoUrl) {
  const GURL geo_url("geo:37.7749,-122.4194");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("Chrome", kChromePackageName, true, GURL()));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(
      GetActionResult::HANDLE_URL_IN_ARC,
      GetActionForTesting(geo_url, handlers, no_selection, &url_and_package));
  EXPECT_EQ(geo_url, url_and_package.first);
  EXPECT_EQ(kChromePackageName, url_and_package.second);
}

// Tests that OPEN_URL_IN_CHROME is returned when a handler with a fallback http
// URL and kChromePackageName is passed to GetAction, even if the handler is not
// a preferred one.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("Chrome", kChromePackageName, false, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::OPEN_URL_IN_CHROME,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(kChromePackageName, url_and_package.second);
}

// Tests the same with https and is_preferred == true.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOnePreferredFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=https://zxing.org;end");
  const GURL fallback_url("https://zxing.org");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("Chrome", kChromePackageName, true, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::OPEN_URL_IN_CHROME,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(kChromePackageName, url_and_package.second);
}

// Tests that ASK_USER is returned when two handlers with fallback URLs are
// passed to GetAction. This may happen when the user has installed a 3rd party
// browser app, and then clicks a intent: URI with a http fallback.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithTwoFallbackUrls) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Other browser", "com.other.browser", false, fallback_url));
  handlers.push_back(Create("Chrome", kChromePackageName, false, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::ASK_USER,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
}

// Tests the same but set Chrome as a preferred app. In this case, ASK_USER
// shouldn't be returned.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsChromePreferred) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Other browser", "com.other.browser", false, fallback_url));
  handlers.push_back(Create("Chrome", kChromePackageName, true, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::OPEN_URL_IN_CHROME,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(kChromePackageName, url_and_package.second);
}

// Tests the same but set "other browser" as a preferred app. In this case,
// ASK_USER shouldn't be returned either.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsOtherBrowserPreferred) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string package_name = "com.other.browser";

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("Other browser", package_name, true, fallback_url));
  handlers.push_back(Create("Chrome", kChromePackageName, false, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(package_name, url_and_package.second);
}

// Tests the same but set Chrome as a user-selected app.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsChromeSelected) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Other browser", "com.other.browser", false, fallback_url));
  handlers.push_back(Create("Chrome", kChromePackageName, false, fallback_url));

  constexpr size_t kSelection = 1;  // Chrome
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::OPEN_URL_IN_CHROME,
            GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                                &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(kChromePackageName, url_and_package.second);
}

// Tests the same but set "other browser" as a preferred app.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoFallbackUrlsOtherBrowserSelected) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=http://zxing.org;end");
  const GURL fallback_url("http://zxing.org");
  const std::string package_name = "com.other.browser";

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Other browser", package_name, false, fallback_url));
  handlers.push_back(Create("Chrome", kChromePackageName, false, fallback_url));

  constexpr size_t kSelection = 0;  // the other browser
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                                &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(package_name, url_and_package.second);
}

// Tests that ASK_USER is returned when a handler with a fallback market: URL
// is passed to GetAction.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithOneMarketFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Play Store", "com.google.play.store", false, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::ASK_USER,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
}

// Tests the same but with is_preferred == true.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOnePreferredMarketFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Play Store", play_store_package_name, true, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(play_store_package_name, url_and_package.second);
}

// Tests the same but with an app_seleteced_index.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithOneSelectedMarketFallbackUrl) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Play Store", play_store_package_name, false, fallback_url));

  constexpr size_t kSelection = 0;
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                                &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(play_store_package_name, url_and_package.second);
}

// Tests that ASK_USER is returned when two handlers with fallback market: URLs
// are passed to GetAction. Unlike the two browsers case, this rarely happens on
// the user's device, though.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithTwoMarketFallbackUrls) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Play Store", "com.google.play.store", false, fallback_url));
  handlers.push_back(
      Create("Other Store app", "com.other.play.store", false, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::ASK_USER,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
}

// Tests the same, but make the second handler a preferred one.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoMarketFallbackUrlsOnePreferred) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Other Store app", "com.other.play.store", false, fallback_url));
  handlers.push_back(
      Create("Play Store", play_store_package_name, true, fallback_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(play_store_package_name, url_and_package.second);
}

// Tests the same, but make the second handler a selected one.
TEST(ArcExternalProtocolDialogTest,
     TestGetActionWithTwoMarketFallbackUrlsOneSelected) {
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;end");
  const GURL fallback_url("market://details?id=com.google.abc");
  const std::string play_store_package_name = "com.google.play.store";

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(
      Create("Other Store app", "com.other.play.store", false, fallback_url));
  handlers.push_back(
      Create("Play Store", play_store_package_name, false, fallback_url));

  const size_t kSelection = 1;  // Play Store
  std::pair<GURL, std::string> url_and_package;
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers, kSelection,
                                &url_and_package));
  EXPECT_EQ(fallback_url, url_and_package.first);
  EXPECT_EQ(play_store_package_name, url_and_package.second);
}

// Tests the case where geo: URL is returned as a fallback. This should never
// happen because intent_helper ignores such a fallback, but just in case.
// GetAction shouldn't crash at least.
TEST(ArcExternalProtocolDialogTest, TestGetActionWithGeoUrlAsFallback) {
  // Note: geo: as a browser fallback is banned in the production code.
  const GURL intent_url_with_fallback(
      "intent://scan/#Intent;scheme=abc;package=com.google.abc;"
      "S.browser_fallback_url=geo:37.7749,-122.4194;end");
  const GURL geo_url("geo:37.7749,-122.4194");

  mojo::Array<mojom::IntentHandlerInfoPtr> handlers;
  handlers.push_back(Create("Chrome", kChromePackageName, true, geo_url));

  const size_t no_selection = handlers.size();
  std::pair<GURL, std::string> url_and_package;
  // GetAction shouldn't return OPEN_URL_IN_CHROME because Chrome doesn't
  // directly support geo:.
  EXPECT_EQ(GetActionResult::HANDLE_URL_IN_ARC,
            GetActionForTesting(intent_url_with_fallback, handlers,
                                no_selection, &url_and_package));
  EXPECT_EQ(geo_url, url_and_package.first);
  EXPECT_EQ(kChromePackageName, url_and_package.second);
}

// Test that ShouldIgnoreNavigation accepts CLIENT_REDIRECT qualifier but does
// not other ones.
TEST(ArcExternalProtocolDialogTest, TestShouldIgnoreNavigation) {
  EXPECT_FALSE(ShouldIgnoreNavigationForTesting(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK)));
  EXPECT_FALSE(ShouldIgnoreNavigationForTesting(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
  EXPECT_TRUE(ShouldIgnoreNavigationForTesting(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT |
      ui::PAGE_TRANSITION_HOME_PAGE)));
  EXPECT_TRUE(ShouldIgnoreNavigationForTesting(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_HOME_PAGE)));
}

}  // namespace arc
