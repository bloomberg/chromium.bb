// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"

using chrome_test_util::AssertMainTabCount;
using chrome_test_util::TapWebViewElementWithId;
using chrome_test_util::WebViewContainingText;
using web::test::HttpServer;

namespace {
// Test link text and ids.
const char kNamedWindowLink[] = "openWindowWithName";
const char kUnnamedWindowLink[] = "openWindowNoName";

// Web view text that indicates window's closed state.
const char kWindow2NeverOpen[] = "window2.closed: never opened";
const char kWindow1Open[] = "window1.closed: false";
const char kWindow2Open[] = "window2.closed: false";
const char kWindow1Closed[] = "window1.closed: true";
const char kWindow2Closed[] = "window2.closed: true";

}  // namespace

// Test case for child windows opened by DOM.
@interface ChildWindowOpenByDOMTestCase : ChromeTestCase
@end

@implementation ChildWindowOpenByDOMTestCase

+ (void)setUp {
  [super setUp];
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
  web::test::SetUpFileBasedHttpServer();
}

+ (void)tearDown {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_DEFAULT);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  // Open the test page. There should only be one tab open.
  const char kChildWindowTestURL[] =
      "http://ios/testing/data/http_server_files/window_proxy.html";
  [ChromeEarlGrey loadURL:HttpServer::MakeUrl(kChildWindowTestURL)];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kNamedWindowLink)]
      assertWithMatcher:grey_notNil()];
  AssertMainTabCount(1);
}

// Tests that multiple calls to window.open() with the same window name returns
// the same window object.
- (void)test2ChildWindowsWithName {
  // Open two windows with the same name.
  TapWebViewElementWithId(kNamedWindowLink);
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  TapWebViewElementWithId(kNamedWindowLink);
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Check that they're the same window.
  TapWebViewElementWithId("compareNamedWindows");
  const char kWindowsEqualText[] = "named windows equal: true";
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindowsEqualText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that multiple calls to window.open() with no window name passed in
// returns a unique window object each time.
- (void)test2ChildWindowsWithoutName {
  // Open two unnamed windows.
  TapWebViewElementWithId(kUnnamedWindowLink);
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  TapWebViewElementWithId(kUnnamedWindowLink);
  AssertMainTabCount(3);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Check that they aren't the same window object.
  TapWebViewElementWithId("compareUnnamedWindows");
  const char kWindowsEqualText[] = "unnamed windows equal: false";
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindowsEqualText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that calling window.open() with a name returns a different window
// object than a subsequent call to window.open() without a name.
- (void)testChildWindowsWithAndWithoutName {
  // Open a named window.
  TapWebViewElementWithId(kNamedWindowLink);
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Open an unnamed window.
  TapWebViewElementWithId(kUnnamedWindowLink);
  AssertMainTabCount(3);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Check that they aren't the same window object.
  TapWebViewElementWithId("compareNamedAndUnnamedWindows");
  const char kWindowsEqualText[] = "named and unnamed equal: false";
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindowsEqualText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that window.closed is correctly set to true when the corresponding tab
// is closed. Verifies that calling window.open() multiple times with the same
// name returns the same window object, and thus closing the corresponding tab
// results in window.closed being set to true for all references to the window
// object for that tab.
- (void)testWindowClosedWithName {
  TapWebViewElementWithId("openWindowWithName");
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Check that named window 1 is opened and named window 2 isn't.
  const char kCheckWindow1Link[] = "checkNamedWindow1Closed";
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kCheckWindow1Link)]
      assertWithMatcher:grey_notNil()];
  TapWebViewElementWithId(kCheckWindow1Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow1Open)]
      assertWithMatcher:grey_notNil()];
  const char kCheckWindow2Link[] = "checkNamedWindow2Closed";
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2NeverOpen)]
      assertWithMatcher:grey_notNil()];

  // Open another window with the same name. Check that named window 2 is now
  // opened.
  TapWebViewElementWithId("openWindowWithName");
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2Open)]
      assertWithMatcher:grey_notNil()];

  // Close the opened window. Check that named window 1 and 2 are both closed.
  chrome_test_util::CloseTabAtIndex(1);
  AssertMainTabCount(1);
  TapWebViewElementWithId(kCheckWindow1Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow1Closed)]
      assertWithMatcher:grey_notNil()];
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2Closed)]
      assertWithMatcher:grey_notNil()];
}

// Tests that closing a tab will set window.closed to true for only
// corresponding window object and not for any other window objects.
- (void)testWindowClosedWithoutName {
  TapWebViewElementWithId("openWindowNoName");
  AssertMainTabCount(2);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Check that unnamed window 1 is opened and unnamed window 2 isn't.
  const char kCheckWindow1Link[] = "checkUnnamedWindow1Closed";
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kCheckWindow1Link)]
      assertWithMatcher:grey_notNil()];
  TapWebViewElementWithId(kCheckWindow1Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow1Open)]
      assertWithMatcher:grey_notNil()];
  const char kCheckWindow2Link[] = "checkUnnamedWindow2Closed";
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2NeverOpen)]
      assertWithMatcher:grey_notNil()];

  // Open another unnamed window. Check that unnamed window 2 is now opened.
  TapWebViewElementWithId("openWindowNoName");
  AssertMainTabCount(3);
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2Open)]
      assertWithMatcher:grey_notNil()];

  // Close the first opened window. Check that unnamed window 1 is closed and
  // unnamed window 2 is still open.
  chrome_test_util::CloseTabAtIndex(1);
  TapWebViewElementWithId(kCheckWindow1Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow1Closed)]
      assertWithMatcher:grey_notNil()];
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2Open)]
      assertWithMatcher:grey_notNil()];

  // Close the second opened window. Check that unnamed window 2 is closed.
  chrome_test_util::CloseTabAtIndex(1);
  TapWebViewElementWithId(kCheckWindow2Link);
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kWindow2Closed)]
      assertWithMatcher:grey_notNil()];
}

@end
