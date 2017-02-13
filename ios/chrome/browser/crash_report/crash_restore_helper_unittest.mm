// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/crash_restore_helper.h"
#import "ios/chrome/browser/sessions/session_service.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Return;

@interface CrashRestoreHelper (Test)
- (NSString*)sessionBackupPath;
@end

namespace {

class CrashRestoreHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    otr_chrome_browser_state_ =
        chrome_browser_state_->GetOffTheRecordChromeBrowserState();
    helper_ = [[CrashRestoreHelper alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ios::ChromeBrowserState* otr_chrome_browser_state_;
  CrashRestoreHelper* helper_;
};

TEST_F(CrashRestoreHelperTest, MoveAsideTest) {
  NSString* backupPath = [helper_ sessionBackupPath];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  [fileManager removeItemAtPath:backupPath error:NULL];

  NSData* data = [NSData dataWithBytes:"hello" length:5];
  SessionServiceIOS* sessionService = [SessionServiceIOS sharedService];
  NSString* profileStashPath =
      base::SysUTF8ToNSString(chrome_browser_state_->GetStatePath().value());
  NSString* sessionPath =
      [sessionService sessionFilePathForDirectory:profileStashPath];
  [fileManager createFileAtPath:sessionPath contents:data attributes:nil];
  NSString* otrProfileStashPath = base::SysUTF8ToNSString(
      otr_chrome_browser_state_->GetStatePath().value());
  NSString* otrSessionPath =
      [sessionService sessionFilePathForDirectory:otrProfileStashPath];
  [fileManager createFileAtPath:otrSessionPath contents:data attributes:nil];

  [helper_ moveAsideSessionInformation];

  EXPECT_EQ(NO, [fileManager fileExistsAtPath:sessionPath]);
  EXPECT_EQ(NO, [fileManager fileExistsAtPath:otrSessionPath]);
  EXPECT_EQ(YES, [fileManager fileExistsAtPath:backupPath]);
  [fileManager removeItemAtPath:backupPath error:NULL];
}

}  // namespace
