// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/nullable_mailto_url_rewriter.h"

#import "ios/chrome/browser/web/fake_mailto_handler_helpers.h"
#import "ios/chrome/browser/web/mailto_handler_system_mail.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class NullableMailtoURLRewriterTest : public PlatformTest {
 public:
  NullableMailtoURLRewriterTest() {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:[NullableMailtoURLRewriter userDefaultsKey]];
  }
};

// Tests that a new instance has expected properties and behaviors.
TEST_F(NullableMailtoURLRewriterTest, TestStandardInstance) {
  NullableMailtoURLRewriter* rewriter =
      [NullableMailtoURLRewriter mailtoURLRewriterWithStandardHandlers];
  EXPECT_TRUE(rewriter);

  NSArray<MailtoHandler*>* handlers = [rewriter defaultHandlers];
  EXPECT_GE([handlers count], 1U);
  for (MailtoHandler* handler in handlers) {
    ASSERT_TRUE(handler);
    NSString* appStoreID = [handler appStoreID];
    NSString* expectedDefaultAppID =
        [handler isAvailable] ? appStoreID : [MailtoURLRewriter systemMailApp];
    [rewriter setDefaultHandlerID:appStoreID];
    EXPECT_NSEQ(expectedDefaultAppID, [rewriter defaultHandlerID]);
    MailtoHandler* foundHandler = [rewriter defaultHandlerByID:appStoreID];
    EXPECT_NSEQ(handler, foundHandler);
  }
}

// If Gmail is not installed, rewriter defaults to system Mail app.
TEST_F(NullableMailtoURLRewriterTest, TestNoGmailInstalled) {
  NullableMailtoURLRewriter* rewriter =
      [[NullableMailtoURLRewriter alloc] init];
  [rewriter setDefaultHandlers:@[
    [[MailtoHandlerSystemMail alloc] init],
    [[FakeMailtoHandlerGmailNotInstalled alloc] init]
  ]];
  EXPECT_NSEQ([MailtoURLRewriter systemMailApp], [rewriter defaultHandlerID]);
}

// If Gmail is installed but user has not made a choice, there is no default
// mail app.
TEST_F(NullableMailtoURLRewriterTest, TestWithGmailChoiceNotMade) {
  NullableMailtoURLRewriter* rewriter =
      [[NullableMailtoURLRewriter alloc] init];
  [rewriter setDefaultHandlers:@[
    [[MailtoHandlerSystemMail alloc] init],
    [[FakeMailtoHandlerGmailInstalled alloc] init]
  ]];
  EXPECT_FALSE([rewriter defaultHandlerID]);
}

// Tests that it is possible to unset the default handler.
TEST_F(NullableMailtoURLRewriterTest, TestUnsetDefaultHandler) {
  NullableMailtoURLRewriter* rewriter =
      [[NullableMailtoURLRewriter alloc] init];
  MailtoHandler* gmailInstalled =
      [[FakeMailtoHandlerGmailInstalled alloc] init];
  MailtoHandler* systemMail = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[ systemMail, gmailInstalled ]];
  [rewriter setDefaultHandlerID:[systemMail appStoreID]];
  EXPECT_NSEQ([systemMail appStoreID], [rewriter defaultHandlerID]);
  [rewriter setDefaultHandlerID:nil];
  EXPECT_FALSE([rewriter defaultHandlerID]);
}

// If Gmail was installed and user has made a choice, then Gmail is uninstalled.
// The default returns to system Mail app.
TEST_F(NullableMailtoURLRewriterTest, TestWithGmailUninstalled) {
  NullableMailtoURLRewriter* rewriter =
      [[NullableMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];
  [rewriter setDefaultHandlerID:[fakeGmailHandler appStoreID]];
  EXPECT_NSEQ([fakeGmailHandler appStoreID], [rewriter defaultHandlerID]);

  rewriter = [[NullableMailtoURLRewriter alloc] init];
  fakeGmailHandler = [[FakeMailtoHandlerGmailNotInstalled alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];
  EXPECT_NSEQ([MailtoURLRewriter systemMailApp], [rewriter defaultHandlerID]);
}

// If Gmail is installed but system Mail app has been chosen by user as the
// default mail handler app. Then Gmail is uninstalled. User's choice of system
// Mail app remains unchanged and will persist through a re-installation of
// Gmail.
TEST_F(NullableMailtoURLRewriterTest,
       TestSystemMailAppChosenSurviveGmailUninstall) {
  // Initial state of system Mail app explicitly chosen.
  NullableMailtoURLRewriter* rewriter =
      [[NullableMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[
    systemMailHandler, [[FakeMailtoHandlerGmailInstalled alloc] init]
  ]];
  [rewriter setDefaultHandlerID:[systemMailHandler appStoreID]];
  EXPECT_NSEQ([systemMailHandler appStoreID], [rewriter defaultHandlerID]);

  // Gmail is installed.
  rewriter = [[NullableMailtoURLRewriter alloc] init];
  [rewriter setDefaultHandlers:@[
    systemMailHandler, [[FakeMailtoHandlerGmailNotInstalled alloc] init]
  ]];
  EXPECT_NSEQ([systemMailHandler appStoreID], [rewriter defaultHandlerID]);

  // Gmail is installed again.
  rewriter = [[NullableMailtoURLRewriter alloc] init];
  [rewriter setDefaultHandlers:@[
    systemMailHandler, [[FakeMailtoHandlerGmailInstalled alloc] init]
  ]];
  EXPECT_NSEQ([systemMailHandler appStoreID], [rewriter defaultHandlerID]);
}
