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
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/public/web_state/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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

  std::unique_ptr<web::WebState> CreateWebState(
      bool created_with_opener) const {
    web::WebState::CreateParams params(chrome_browser_state_.get());
    params.created_with_opener = created_with_opener;
    return web::WebState::Create(params);
  }

  web::TestWebThreadBundle thread_bundle_;
  // TODO(crbug.com/661639): Switch to TestBrowserState once this test is moved
  // to be in the ios_web_unittests target.
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(SessionWindowIOSTest, InitEmpty) {
  base::scoped_nsobject<SessionWindowIOS> session_window(
      [[SessionWindowIOS alloc] init]);
  EXPECT_TRUE(session_window.get() != nil);
}

TEST_F(SessionWindowIOSTest, InitAddingSessions) {
  std::unique_ptr<web::WebState> web_state_1 =
      CreateWebState(false /*created_with_opener*/);
  std::unique_ptr<web::WebState> web_state_2 =
      CreateWebState(false /*created_with_opener*/);
  base::scoped_nsobject<SessionWindowIOS> session_window(
      [[SessionWindowIOS alloc] init]);
  [session_window
      addSerializedSessionStorage:web_state_1->BuildSessionStorage()];
  [session_window
      addSerializedSessionStorage:web_state_2->BuildSessionStorage()];
  [session_window setSelectedIndex:1];

  EXPECT_TRUE(session_window.get() != nil);
  EXPECT_EQ(2U, session_window.get().sessions.count);
  [session_window clearSessions];
  EXPECT_EQ(0U, session_window.get().sessions.count);
}

TEST_F(SessionWindowIOSTest, CodingEncoding) {
  base::scoped_nsobject<SessionWindowIOS> session_window(
      [[SessionWindowIOS alloc] init]);

  std::unique_ptr<web::WebState> web_state_1 =
      CreateWebState(true /*created_with_opener*/);
  std::unique_ptr<web::WebState> web_state_2 =
      CreateWebState(false /*created_with_opener*/);
  [session_window
      addSerializedSessionStorage:web_state_1->BuildSessionStorage()];
  [session_window
      addSerializedSessionStorage:web_state_2->BuildSessionStorage()];

  [session_window setSelectedIndex:1];

  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:session_window];
  EXPECT_TRUE(data != nil);
  base::scoped_nsobject<SessionWindowUnarchiver> unarchiver(
      [[SessionWindowUnarchiver alloc]
          initForReadingWithData:data
                    browserState:chrome_browser_state_.get()]);
  SessionWindowIOS* unarchivedObj = [unarchiver decodeObjectForKey:@"root"];
  EXPECT_TRUE(unarchivedObj != nil);
  EXPECT_EQ(unarchivedObj.selectedIndex, session_window.get().selectedIndex);
  NSArray* sessions = unarchivedObj.sessions;
  ASSERT_EQ(2U, sessions.count);
  CRWSessionStorage* unarchivedSession1 = sessions[0];
  EXPECT_TRUE(unarchivedSession1.hasOpener);

  CRWSessionStorage* unarchivedSession2 = sessions[1];
  EXPECT_FALSE(unarchivedSession2.hasOpener);
}

}  // anonymous namespace
