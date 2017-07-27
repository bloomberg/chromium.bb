// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Timeout to use when waiting for a condition to be true.
const CFTimeInterval kConditionTimeout = 4.0;

// Returns the URL for the HTML that is used for testing purposes in this file.
GURL GetTestUrl() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/"
      "browsing_prevent_default_test_page.html");
}

// ScopedBlockPopupsPref modifies the block popups preference and resets the
// preference to its original value when this object goes out of scope.
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

// Tests that the javascript preventDefault() function correctly prevents new
// tabs from opening or navigation from occurring.
@interface BrowsingPreventDefaultTestCase : ChromeTestCase
@end

@implementation BrowsingPreventDefaultTestCase

// Helper function to tap a link and verify that the URL did not change and no
// new tabs were opened.
- (void)runTestAndVerifyNoNavigationForLinkID:(const std::string&)linkID {
  // Disable popup blocking, because that will mask failures that try to open
  // new tabs.
  ScopedBlockPopupsPref scoper(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();

  const GURL testURL = GetTestUrl();
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on the test link and wait for the page to display "Click done", as an
  // indicator that the element was tapped.
  chrome_test_util::TapWebViewElementWithId(linkID);
  [ChromeEarlGrey waitForWebViewContainingText:"Click done"];

  // Check that no navigation occurred and no new tabs were opened.
  [ChromeEarlGrey waitForMainTabCount:1];
  const GURL& currentURL =
      chrome_test_util::GetCurrentWebState()->GetVisibleURL();
  GREYAssert(currentURL == testURL, @"Page navigated unexpectedly %s",
             currentURL.spec().c_str());
}

// Taps a link with onclick="event.preventDefault()" and target="_blank" and
// verifies that the URL didn't change and no tabs were opened.
- (void)testPreventDefaultOverridesTargetBlank {
  const std::string linkID =
      "webScenarioBrowsingLinkPreventDefaultOverridesTargetBlank";
  [self runTestAndVerifyNoNavigationForLinkID:linkID];
}

// Tests clicking a link with target="_blank" and event 'preventDefault()' and
// 'stopPropagation()' does not change the current URL nor open a new tab.
- (void)testPreventDefaultOverridesStopPropagation {
  const std::string linkID =
      "webScenarioBrowsingLinkPreventDefaultOverridesStopPropagation";
  [self runTestAndVerifyNoNavigationForLinkID:linkID];
}

// Tests clicking a link with event 'preventDefault()' and URL loaded by
// JavaScript does not open a new tab, but does navigate to the URL.
- (void)testPreventDefaultOverridesWindowOpen {
  // Disable popup blocking, because that will mask failures that try to open
  // new tabs.
  ScopedBlockPopupsPref scoper(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();

  const GURL testURL = GetTestUrl();
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Tap on the test link.
  const std::string linkID =
      "webScenarioBrowsingLinkPreventDefaultOverridesWindowOpen";
  chrome_test_util::TapWebViewElementWithId(linkID);

  // TODO(crbug.com/747434): It seems like the text 'Click done' isn't visible
  // long enough on iOS11.  It's possible this is an issue in Earl Grey, iOS11,
  // or just the extra text check is unnecessary.
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    [ChromeEarlGrey waitForWebViewContainingText:"Click done"];
  }

  // Check that the tab navigated to about:blank and no new tabs were opened.
  [[GREYCondition
      conditionWithName:@"Wait for navigation to about:blank"
                  block:^BOOL {
                    const GURL& currentURL =
                        chrome_test_util::GetCurrentWebState()->GetVisibleURL();
                    return currentURL == GURL(url::kAboutBlankURL);
                  }] waitWithTimeout:kConditionTimeout];
  [ChromeEarlGrey waitForMainTabCount:1];
}

@end
