// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/history_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"
#include "url/gurl.h"

using chrome_test_util::WebViewContainingText;
using web::test::HttpServer;

namespace {

// First page for cache testing.
const char kCacheTestFirstPageURL[] = "http://cacheTestFirstPage";

// Second page for cache testing.
const char kCacheTestSecondPageURL[] = "http://cacheTestSecondPage";

// Third page for cache testing.
const char kCacheTestThirdPageURL[] = "http://cacheTestThirdPage";

// ID for HTML hyperlink element.
const char kCacheTestLinkID[] = "cache-test-link-id";

// Response provider for cache testing that provides server hit count and
// cache-control request header.
class CacheTestResponseProvider : public web::DataResponseProvider {
 public:
  CacheTestResponseProvider()
      : first_page_url_(HttpServer::MakeUrl(kCacheTestFirstPageURL)),
        second_page_url_(HttpServer::MakeUrl(kCacheTestSecondPageURL)),
        third_page_url_(HttpServer::MakeUrl(kCacheTestThirdPageURL)) {}
  ~CacheTestResponseProvider() override {}

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override {
    return request.url == first_page_url_ || request.url == second_page_url_ ||
           request.url == third_page_url_;
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    hit_counter_++;
    std::string cache_control_header;
    if (request.headers.HasHeader("Cache-Control")) {
      request.headers.GetHeader("Cache-Control", &cache_control_header);
    }
    *headers = web::ResponseProvider::GetDefaultResponseHeaders();

    if (request.url == first_page_url_) {
      *response_body = base::StringPrintf(
          "<p>First Page</p>"
          "<p>serverHitCounter: %d</p>"
          "<p>cacheControl: %s</p>"
          "<a href='%s' id='%s'>link to second page</a>",
          hit_counter_, cache_control_header.c_str(),
          second_page_url_.spec().c_str(), kCacheTestLinkID);

    } else if (request.url == second_page_url_) {
      *response_body = base::StringPrintf(
          "<p>Second Page</p>"
          "<p>serverHitCounter: %d</p>"
          "<p>cacheControl: %s</p>",
          hit_counter_, cache_control_header.c_str());
    } else if (request.url == third_page_url_) {
      *response_body = base::StringPrintf(
          "<p>Third Page</p>"
          "<p>serverHitCounter: %d</p>"
          "<p>cacheControl: %s</p>"
          "<a href='%s' id='%s' target='_blank'>"
          "link to first page in new tab</a>",
          hit_counter_, cache_control_header.c_str(),
          first_page_url_.spec().c_str(), kCacheTestLinkID);
    } else {
      NOTREACHED();
    }
  }

 private:
  // A number that counts requests that have reached the server.
  int hit_counter_ = 0;

  // URLs for three test pages.
  GURL first_page_url_;
  GURL second_page_url_;
  GURL third_page_url_;
};

// ScopedBlockPopupsPref modifies the block popups preference and resets the
// preference to its original value when this object goes out of scope.
// TODO(crbug.com/638674): Evaluate if this can move to shared code.
class ScopedBlockPopupsPref {
 public:
  explicit ScopedBlockPopupsPref(ContentSetting setting) {
    original_setting_ = GetPrefValue();
    SetPrefValue(setting);
  }
  ~ScopedBlockPopupsPref() { SetPrefValue(original_setting_); }

 private:
  // Gets the current value of the preference.
  ContentSetting GetPrefValue() {
    ContentSetting popupSetting =
        ios::HostContentSettingsMapFactory::GetForBrowserState(
            chrome_test_util::GetOriginalBrowserState())
            ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, NULL);
    return popupSetting;
  }

  // Sets the preference to the given value.
  void SetPrefValue(ContentSetting setting) {
    DCHECK(setting == CONTENT_SETTING_BLOCK ||
           setting == CONTENT_SETTING_ALLOW);
    ios::ChromeBrowserState* state =
        chrome_test_util::GetOriginalBrowserState();
    ios::HostContentSettingsMapFactory::GetForBrowserState(state)
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, setting);
  }

  // Saves the original pref setting so that it can be restored when the scoper
  // is destroyed.
  ContentSetting original_setting_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBlockPopupsPref);
};

}  // namespace

// Tests the browser cache behavior when navigating to cached pages.
@interface CacheTestCase : ChromeTestCase
@end

@implementation CacheTestCase

// Reloads the web view and waits for the loading to complete.
// TODO(crbug.com/638674): Evaluate if this can move to shared code
- (void)reloadPage {
  base::scoped_nsobject<GenericChromeCommand> reloadCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_RELOAD]);
  chrome_test_util::RunCommandWithActiveViewController(reloadCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Navigates back to the previous webpage.
// TODO(crbug.com/638674): Evaluate if this can move to shared code.
- (void)goBack {
  base::scoped_nsobject<GenericChromeCommand> backCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_BACK]);
  chrome_test_util::RunCommandWithActiveViewController(backCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Tests caching behavior on navigate back and page reload. Navigate back should
// use the cached page. Page reload should use cache-control in the request
// header and show updated page.
- (void)testCachingBehaviorOnNavigateBackAndPageReload {
  web::test::SetUpHttpServer(base::MakeUnique<CacheTestResponseProvider>());

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);

  // 1st hit to server. Verify that the server has the correct hit count.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 1")]
      assertWithMatcher:grey_notNil()];

  // Navigate to another page. 2nd hit to server.
  chrome_test_util::TapWebViewElementWithId(kCacheTestLinkID);
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 2")]
      assertWithMatcher:grey_notNil()];

  // Navigate back. This should not hit the server. Verify the page has been
  // loaded from cache. The serverHitCounter will remain the same.
  [self goBack];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 1")]
      assertWithMatcher:grey_notNil()];

  // Reload page. 3rd hit to server. Verify that page reload causes the
  // hitCounter to show updated value.
  [self reloadPage];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 3")]
      assertWithMatcher:grey_notNil()];

  // Verify that page reload causes Cache-Control value to be sent with request.
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("cacheControl: max-age=0")]
      assertWithMatcher:grey_notNil()];
}

// Tests caching behavior when opening new tab. New tab should not use the
// cached page.
- (void)testCachingBehaviorOnOpenNewTab {
  web::test::SetUpHttpServer(base::MakeUnique<CacheTestResponseProvider>());

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);
  const GURL cacheTestThirdPageURL =
      HttpServer::MakeUrl(kCacheTestThirdPageURL);

  // 1st hit to server. Verify title and hitCount.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("First Page")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 1")]
      assertWithMatcher:grey_notNil()];

  // 2nd hit to server. Verify hitCount.
  [ChromeEarlGrey loadURL:cacheTestThirdPageURL];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 2")]
      assertWithMatcher:grey_notNil()];

  // Open the first page in a new tab. Verify that cache was not used. Must
  // first allow popups.
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);
  chrome_test_util::TapWebViewElementWithId(kCacheTestLinkID);
  chrome_test_util::AssertMainTabCount(2);
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("First Page")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 3")]
      assertWithMatcher:grey_notNil()];
}

// Tests that cache is not used when selecting omnibox suggested website, even
// though cache for that website exists.
- (void)testCachingBehaviorOnSelectOmniboxSuggestion {
  web::test::SetUpHttpServer(base::MakeUnique<CacheTestResponseProvider>());

  // Clear the history to ensure expected omnibox autocomplete results.
  chrome_test_util::ClearBrowsingHistory();

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);

  // 1st hit to server. Verify title and hitCount.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("First Page")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 1")]
      assertWithMatcher:grey_notNil()];

  // Type a search into omnnibox and select the first suggestion (second row)
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"cachetestfirstpage")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"omnibox suggestion 1")]
      performAction:grey_tap()];

  // Verify title and hitCount. Cache should not be used.
  [[EarlGrey selectElementWithMatcher:WebViewContainingText("First Page")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:WebViewContainingText("serverHitCounter: 2")]
      assertWithMatcher:grey_notNil()];
}

@end
