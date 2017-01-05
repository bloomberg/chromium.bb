// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsobject.h"
#include "base/timer/elapsed_timer.h"
#include "ios/chrome/test/base/perf_test_ios.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/js/page_script_util.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace {

// Class for testing early page script injection into WKWebView.
// TODO(crbug.com/583218): improve this test to use WKUserScript injections.
class EarlyPageScriptPerfTest : public PerfTest {
 protected:
  EarlyPageScriptPerfTest() : PerfTest("Early Page Script for WKWebView") {
    web_view_.reset([web::BuildWKWebView(CGRectZero, &browser_state_) retain]);
  }

  // Injects early script into WKWebView.
  void InjectEarlyScript() {
    web::ExecuteJavaScript(web_view_, web::GetEarlyPageScript());
  }

  // BrowserState required for web view creation.
  web::TestBrowserState browser_state_;
  // WKWebView to test scripts injections.
  base::scoped_nsobject<WKWebView> web_view_;
};

// Tests script loading time.
TEST_F(EarlyPageScriptPerfTest, ScriptLoading) {
  RepeatTimedRuns("Loading",
                  ^base::TimeDelta(int) {
                    base::ElapsedTimer timer;
                    web::GetEarlyPageScript();
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
