// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_window_id_manager.h"

#import <WebKit/WebKit.h>

#import "ios/web/public/test/js_test_util.h"
#import "ios/web/web_state/js/page_script_util.h"
#import "testing/gtest_mac.h"

namespace web {

// Tests that window ID injection by a second manager results in a different
// window ID.
TEST(JSWindowIDManagerTest, WindowIDDifferentManager) {
  // Inject the first manager.
  WKWebView* web_view = [[[WKWebView alloc] init] autorelease];
  ExecuteJavaScript(web_view, GetEarlyPageScript());

  CRWJSWindowIDManager* manager =
      [[[CRWJSWindowIDManager alloc] initWithWebView:web_view] autorelease];
  [manager inject];
  EXPECT_NSEQ([manager windowID],
              ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));

  // Inject the second manager.
  WKWebView* web_view2 = [[[WKWebView alloc] init] autorelease];
  ExecuteJavaScript(web_view2, GetEarlyPageScript());

  CRWJSWindowIDManager* manager2 =
      [[[CRWJSWindowIDManager alloc] initWithWebView:web_view2] autorelease];
  [manager2 inject];
  EXPECT_NSEQ([manager2 windowID],
              ExecuteJavaScript(web_view2, @"window.__gCrWeb.windowId"));

  // Window IDs must be different.
  EXPECT_NSNE([manager windowID], [manager2 windowID]);
}

// Tests that injecting multiple times creates a new window ID.
TEST(JSWindowIDManagerTest, MultipleInjections) {
  WKWebView* web_view = [[[WKWebView alloc] init] autorelease];
  ExecuteJavaScript(web_view, GetEarlyPageScript());

  // First injection.
  CRWJSWindowIDManager* manager =
      [[[CRWJSWindowIDManager alloc] initWithWebView:web_view] autorelease];
  [manager inject];
  NSString* windowID = [manager windowID];
  EXPECT_NSEQ(windowID,
              ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));

  // Second injection.
  [manager inject];
  EXPECT_NSEQ([manager windowID],
              ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));

  EXPECT_NSNE(windowID, [manager windowID]);
}

// Tests that injection will retry if |window.__gCrWeb| is not present.
TEST(JSWindowIDManagerTest, InjectionRetry) {
  WKWebView* web_view = [[[WKWebView alloc] init] autorelease];

  CRWJSWindowIDManager* manager =
      [[[CRWJSWindowIDManager alloc] initWithWebView:web_view] autorelease];
  [manager inject];
  EXPECT_TRUE([manager windowID]);
  EXPECT_FALSE(ExecuteJavaScript(web_view, @"window.__gCrWeb"));

  // Now inject window.__gCrWeb and check if window ID injection retried.
  ExecuteJavaScript(web_view, GetEarlyPageScript());
  EXPECT_NSEQ([manager windowID],
              ExecuteJavaScript(web_view, @"window.__gCrWeb.windowId"));
}

}  // namespace web
