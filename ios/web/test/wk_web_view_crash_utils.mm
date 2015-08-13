// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/wk_web_view_crash_utils.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/web_view_creation_util.h"
#import "third_party/ocmock/OCMock/NSInvocation+OCMAdditions.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Returns an OCMocked WKWebView whose |evaluateJavaScript:stringResultHandler:|
// method has been mocked to execute |block| instead. |block| cannot be nil.
WKWebView* CreateMockWKWebViewWithStubbedJSEvalFunction(
    void (^block)(NSInvocation*)) {
  DCHECK(block);
  web::TestBrowserState browser_state;
  base::scoped_nsobject<WKWebView> webView(
      web::CreateWKWebView(CGRectZero, &browser_state));
  id mockWebView = [OCMockObject partialMockForObject:webView];
  [[[mockWebView stub] andDo:^void(NSInvocation* invocation) {
      block(invocation);
  }] evaluateJavaScript:OCMOCK_ANY completionHandler:OCMOCK_ANY];
  return [mockWebView retain];
}

}  // namespace

namespace web {

void SimulateWKWebViewCrash(WKWebView* webView) {
  if (base::ios::IsRunningOnIOS9OrLater()) {
    SEL selector = @selector(webViewWebContentProcessDidTerminate:);
    if ([webView.navigationDelegate respondsToSelector:selector]) {
      [webView.navigationDelegate performSelector:selector withObject:webView];
    }
  }
  [webView performSelector:@selector(_processDidExit)];
}

WKWebView* CreateTerminatedWKWebView() {
  id fail = ^void(NSInvocation* invocation) {
      // Always fails with WKErrorWebContentProcessTerminated error.
      NSError* error =
          [NSError errorWithDomain:WKErrorDomain
                              code:WKErrorWebContentProcessTerminated
                          userInfo:nil];

      void (^completionHandler)(id, NSError*) =
          [invocation getArgumentAtIndexAsObject:3];
      completionHandler(nil, error);
  };
  return CreateMockWKWebViewWithStubbedJSEvalFunction(fail);
}

WKWebView* CreateHealthyWKWebView() {
  id succeed = ^void(NSInvocation* invocation) {
      void (^completionHandler)(id, NSError*) =
          [invocation getArgumentAtIndexAsObject:3];
      // Always succceeds with nil result.
      completionHandler(nil, nil);
  };
  return CreateMockWKWebViewWithStubbedJSEvalFunction(succeed);
}

}  // namespace web
