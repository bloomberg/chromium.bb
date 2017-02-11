// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_window.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/public/web_state/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using web::WebStateImpl;

@interface SessionWindowIOS (Testing)

- (void)clearSessions;

@end

namespace {

class SessionWindowIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  WebStateImpl* CreateWebState(NSString* window_name,
                               NSString* opener,
                               BOOL openedByDOM) const {
    WebStateImpl* webState = new WebStateImpl(chrome_browser_state_.get());
    webState->GetNavigationManagerImpl().InitializeSession(window_name, opener,
                                                           openedByDOM, 0);
    return webState;
  }

  web::TestWebThreadBundle thread_bundle_;
  // TODO(crbug.com/661639): Switch to TestBrowserState once this test is moved
  // to be in the ios_web_unittests target.
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(SessionWindowIOSTest, InitEmpty) {
  base::scoped_nsobject<SessionWindowIOS> sessionWindow(
      [[SessionWindowIOS alloc] init]);
  EXPECT_TRUE(sessionWindow.get() != nil);
}

TEST_F(SessionWindowIOSTest, InitAddingSessions) {
  std::unique_ptr<WebStateImpl> webState1(CreateWebState(@"window1", nil, NO));
  std::unique_ptr<WebStateImpl> webState2(CreateWebState(@"window2", nil, NO));
  base::scoped_nsobject<SessionWindowIOS> sessionWindow(
      [[SessionWindowIOS alloc] init]);
  [sessionWindow addSerializedSessionStorage:webState1->BuildSessionStorage()];
  [sessionWindow addSerializedSessionStorage:webState2->BuildSessionStorage()];
  [sessionWindow setSelectedIndex:1];

  EXPECT_TRUE(sessionWindow.get() != nil);
  EXPECT_EQ(2U, sessionWindow.get().sessions.count);
  [sessionWindow clearSessions];
  EXPECT_EQ(0U, sessionWindow.get().sessions.count);
}

TEST_F(SessionWindowIOSTest, CodingEncoding) {
  NSString* windowName1 = @"window1";
  NSString* windowName2 = @"window2";
  base::scoped_nsobject<SessionWindowIOS> sessionWindow(
      [[SessionWindowIOS alloc] init]);

  std::unique_ptr<WebStateImpl> webState1(
      CreateWebState(windowName1, nil, YES));
  NSString* openerId1 =
      webState1->GetNavigationManagerImpl().GetSessionController().openerId;
  std::unique_ptr<WebStateImpl> webState2(CreateWebState(windowName2, nil, NO));
  NSString* openerId2 =
      webState2->GetNavigationManagerImpl().GetSessionController().openerId;
  [sessionWindow addSerializedSessionStorage:webState1->BuildSessionStorage()];
  [sessionWindow addSerializedSessionStorage:webState2->BuildSessionStorage()];

  [sessionWindow setSelectedIndex:1];

  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:sessionWindow];
  EXPECT_TRUE(data != nil);
  base::scoped_nsobject<SessionWindowUnarchiver> unarchiver(
      [[SessionWindowUnarchiver alloc]
          initForReadingWithData:data
                    browserState:chrome_browser_state_.get()]);
  SessionWindowIOS* unarchivedObj = [unarchiver decodeObjectForKey:@"root"];
  EXPECT_TRUE(unarchivedObj != nil);
  EXPECT_EQ(unarchivedObj.selectedIndex, sessionWindow.get().selectedIndex);
  NSArray* sessions = unarchivedObj.sessions;
  ASSERT_EQ(2U, sessions.count);
  CRWSessionStorage* unarchivedSession1 = sessions[0];
  EXPECT_NSEQ(windowName1, unarchivedSession1.windowName);
  EXPECT_NSEQ(openerId1, unarchivedSession1.openerID);
  EXPECT_TRUE(unarchivedSession1.openedByDOM);

  CRWSessionStorage* unarchivedSession2 = sessions[1];
  EXPECT_NSEQ(windowName2, unarchivedSession2.windowName);
  EXPECT_NSEQ(openerId2, unarchivedSession2.openerID);
  EXPECT_FALSE(unarchivedSession2.openedByDOM);
}

}  // anonymous namespace
