// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/page_script_util.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_view_creation_util.h"
#include "testing/gtest_mac.h"

namespace web {
namespace {

// A test fixture for testing the page_script_util methods.
typedef WebTest PageScriptUtilTest;

// Tests that WKWebView early page script is a valid script that injects global
// __gCrWeb object.
TEST_F(PageScriptUtilTest, WKWebViewEarlyPageScript) {
  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(CGRectZero, GetBrowserState()));
  EvaluateJavaScript(web_view, GetEarlyPageScript());
  EXPECT_NSEQ(@"object", EvaluateJavaScript(web_view, @"typeof __gCrWeb"));
}

// Tests that embedder's WKWebView script is included into early script.
TEST_F(PageScriptUtilTest, WKEmbedderScript) {
  GetWebClient()->SetEarlyPageScript(@"__gCrEmbedder = {};");
  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(CGRectZero, GetBrowserState()));
  EvaluateJavaScript(web_view, GetEarlyPageScript());
  EXPECT_NSEQ(@"object", EvaluateJavaScript(web_view, @"typeof __gCrEmbedder"));
}

}  // namespace
}  // namespace web
