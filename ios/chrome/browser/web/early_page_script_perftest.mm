// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/bundle_locations.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/test/base/perf_test_ios.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/js/page_script_util.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Class for testing early page script injection into WKWebView.
// TODO(crbug.com/583218): improve this test to use WKUserScript injections.
class EarlyPageScriptPerfTest : public PerfTest {
 protected:
  EarlyPageScriptPerfTest() : PerfTest("Early Page Script for WKWebView") {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    web_view_ = web::BuildWKWebView(CGRectZero, browser_state());
  }

  // Injects early script into WKWebView.
  void InjectEarlyScript() {
    web::ExecuteJavaScript(web_view_, web::GetEarlyPageScript(browser_state()));
  }

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }

  // BrowserState required for web view creation.
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;
  // WKWebView to test scripts injections.
  WKWebView* web_view_;
};

// Tests script loading time.
TEST_F(EarlyPageScriptPerfTest, ScriptLoading) {
  RepeatTimedRuns("Loading",
                  ^base::TimeDelta(int) {
                    base::ElapsedTimer timer;
                    web::GetEarlyPageScript(browser_state());
                    return timer.Elapsed();
                  },
                  nil);
}

// Tests injection time into a bare web view.
TEST_F(EarlyPageScriptPerfTest, BareWebViewInjection) {
  RepeatTimedRuns("Bare web view injection",
                  ^base::TimeDelta(int) {
                    base::ElapsedTimer timer;
                    InjectEarlyScript();
                    return timer.Elapsed();
                  },
                  nil);
}

}  // namespace
