// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_scheme_handler.h"

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/test/web_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeSchemeTask : NSObject <WKURLSchemeTask>

@property(nonatomic, assign) BOOL receivedData;

@end

@implementation FakeSchemeTask

@synthesize request = _request;

- (void)didReceiveResponse:(NSURLResponse*)response {
}

- (void)didReceiveData:(NSData*)data {
  self.receivedData = YES;
}

- (void)didFinish {
}

- (void)didFailWithError:(NSError*)error {
}

@end

namespace web {

// Test fixture for testing CRWWebUISchemeManager.
class CRWWebUISchemeManagerTest : public WebTest {
 protected:
  CRWWebUISchemeHandler* CreateSchemeHandler() {
    return [[CRWWebUISchemeHandler alloc]
        initWithURLLoaderFactory:GetBrowserState()
                                     ->GetSharedURLLoaderFactory()];
  }
};

// Tests that calling start on the scheme handler returns some data.
TEST_F(CRWWebUISchemeManagerTest, StartTask) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  WKWebView* web_view;
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];
  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];

  // Run the runloop to let the url loader send back an answer.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        // May or may not require |base::RunLoop().RunUntilIdle();| call.
        return url_scheme_task.receivedData;
      }));
}

// Tests that calling stop right after start prevent the handler from returning
// data.
TEST_F(CRWWebUISchemeManagerTest, StopTask) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  WKWebView* web_view;
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];

  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];
  [scheme_handler webView:web_view stopURLSchemeTask:url_scheme_task];

  // Run the runloop to let the url loader send back an answer.
  EXPECT_FALSE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        // May or may not require |base::RunLoop().RunUntilIdle();| call.
        return url_scheme_task.receivedData;
      }));
}
}
