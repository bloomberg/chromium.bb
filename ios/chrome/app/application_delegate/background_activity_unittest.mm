// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/background_activity.h"

#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/browser/crash_report/crash_report_background_uploader.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/metrics/previous_session_info_private.h"
#import "ios/chrome/test/base/scoped_block_swizzler.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using BackgroundActivityTest = PlatformTest;

// Verifies that -application:performFetchWithCompletionHandler: calls the
// browser launcher in background state and uploads the report.
TEST_F(BackgroundActivityTest, performFetchWithCompletionHandler) {
  // Setup.
  [[PreviousSessionInfo sharedInstance] setIsFirstSessionAfterUpgrade:NO];

  // MetricsMediator mock.
  id metrics_mediator_mock =
      [OCMockObject mockForClass:[MetricsMediator class]];
  [[[metrics_mediator_mock stub] andReturnValue:@YES] areMetricsEnabled];
  [[[metrics_mediator_mock stub] andReturnValue:@YES] isUploadingEnabled];

  // BrowserLauncher mock.
  id browser_launcher =
      [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
  [[browser_launcher expect]
      startUpBrowserToStage:INITIALIZATION_STAGE_BACKGROUND];

  // CrashReportBackgroundUploader swizzle.
  __block BOOL crash_report_completion_handler_has_been_called = NO;
  id implementation_block = ^(id self) {
    crash_report_completion_handler_has_been_called = YES;
  };
  ScopedBlockSwizzler crash_report_completion_handler_swizzler(
      [CrashReportBackgroundUploader class],
      @selector(performFetchWithCompletionHandler:), implementation_block);

  // Test.
  [BackgroundActivity application:nil
      performFetchWithCompletionHandler:nil
                        metricsMediator:metrics_mediator_mock
                        browserLauncher:browser_launcher];

  // Check.
  EXPECT_OCMOCK_VERIFY(browser_launcher);
  EXPECT_TRUE(crash_report_completion_handler_has_been_called);
}

// Verifies that -handleEventsForBackgroundURLSession:completionHandler: calls
// the browser launcher in background state.
TEST_F(BackgroundActivityTest, handleEventsForBackgroundURLSession) {
  // Setup.
  // BrowserLauncher mock.
  id browser_launcher =
      [OCMockObject mockForProtocol:@protocol(BrowserLauncher)];
  [[browser_launcher expect]
      startUpBrowserToStage:INITIALIZATION_STAGE_BACKGROUND];

  // Test.
  [BackgroundActivity handleEventsForBackgroundURLSession:nil
                                        completionHandler:^{
                                        }
                                          browserLauncher:browser_launcher];

  // Check.
  EXPECT_OCMOCK_VERIFY(browser_launcher);
}
