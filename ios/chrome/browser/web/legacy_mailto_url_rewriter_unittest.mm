// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/legacy_mailto_url_rewriter.h"

#import "ios/chrome/browser/web/fake_mailto_handler_helpers.h"
#import "ios/chrome/browser/web/mailto_handler_system_mail.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Defines the 3 valid states for ShouldAutoOpenLinks_422689480.
enum {
  kAutoOpenLinksNotSet = 0,
  kAutoOpenLinksNo = 1,
  kAutoOpenLinksYes = 2,
};
NSString* const kLegacyShouldAutoOpenKey = @"ShouldAutoOpenLinks_422689480";
NSString* const kGmailAppStoreID = @"422689480";

}  // namespace

#pragma mark - Unit Test Cases

class LegacyMailtoURLRewriterTest : public PlatformTest {
 public:
  LegacyMailtoURLRewriterTest() {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:[LegacyMailtoURLRewriter userDefaultsKey]];
  }
};

// Tests that a standard instance has the expected values.
TEST_F(LegacyMailtoURLRewriterTest, TestStandardInstance) {
  LegacyMailtoURLRewriter* rewriter =
      [LegacyMailtoURLRewriter mailtoURLRewriterWithStandardHandlers];
  EXPECT_TRUE(rewriter);
  EXPECT_GT([[rewriter defaultHandlerName] length], 0U);
  // ID for system Mail client app must not be an empty string.
  EXPECT_GT([[LegacyMailtoURLRewriter systemMailApp] length], 0U);

  NSArray<MailtoHandler*>* handlers = [rewriter defaultHandlers];
  EXPECT_GE([handlers count], 1U);
  for (MailtoHandler* handler in handlers) {
    ASSERT_TRUE(handler);
    NSString* appStoreID = [handler appStoreID];
    NSString* expectedDefaultAppID =
        [handler isAvailable] ? appStoreID
                              : [LegacyMailtoURLRewriter systemMailApp];
    [rewriter setDefaultHandlerID:appStoreID];
    EXPECT_NSEQ(expectedDefaultAppID, [rewriter defaultHandlerID]);
    MailtoHandler* foundHandler = [rewriter defaultHandlerByID:appStoreID];
    EXPECT_NSEQ(handler, foundHandler);
  }
}

TEST_F(LegacyMailtoURLRewriterTest, TestDefaultsInvalidToSystemMail) {
  // Sets up a LegacyMailtoURLRewriter with 2 MailtoHandler objects:
  // - system-provided Mail client app
  // - Gmail app, but it is not installed
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailNotInstalled alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];
  // Sets the default handler to Gmail (which is not installed). This simulates
  // the situation when Gmail was installed and set as the default handler.
  // Then Gmail app is deleted from the device.
  [rewriter setDefaultHandlerID:[fakeGmailHandler appStoreID]];
  // Verifies that the system-provided Mail app automatically assumes the
  // default handler.
  EXPECT_NSEQ([LegacyMailtoURLRewriter systemMailApp],
              [rewriter defaultHandlerID]);
}

TEST_F(LegacyMailtoURLRewriterTest, TestUserPreferencePersistence) {
  // Sets up a first LegacyMailtoURLRewriter with at least 2 MailtoHandler
  // objects. A faked Gmail handler that is installed must be used or
  // -setDefaultHandlers: will just skip it.
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];

  // Verifies that there must be 2 registered handlers. Then find a
  // MailtoHandler that is not the current default and set that as the new
  // default.
  NSArray<MailtoHandler*>* handlers = [rewriter defaultHandlers];
  ASSERT_GE([handlers count], 2U);
  NSString* initialHandlerID = [rewriter defaultHandlerID];
  NSString* otherHandlerID = nil;
  for (MailtoHandler* handler in handlers) {
    if (![initialHandlerID isEqualToString:[handler appStoreID]]) {
      otherHandlerID = [handler appStoreID];
      break;
    }
  }
  ASSERT_TRUE([otherHandlerID length]);
  [rewriter setDefaultHandlerID:otherHandlerID];

  // Create a new LegacyMailtoURLRewriter object and verify that the current
  // default is the |otherHandlerID| set in the previous step.
  LegacyMailtoURLRewriter* rewriter2 = [[LegacyMailtoURLRewriter alloc] init];
  [rewriter2 setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];
  EXPECT_NSEQ(otherHandlerID, [rewriter2 defaultHandlerID]);
}

TEST_F(LegacyMailtoURLRewriterTest, TestChangeObserver) {
  CountingMailtoURLRewriterObserver* observer =
      [[CountingMailtoURLRewriterObserver alloc] init];
  ASSERT_EQ(0, [observer changeCount]);

  // Sets up a LegacyMailtoURLRewriter object. The default handler is Gmail app
  // because |fakeGmailHandler| reports that it is "installed".
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];
  EXPECT_NSEQ([fakeGmailHandler appStoreID], [rewriter defaultHandlerID]);
  [rewriter setObserver:observer];

  // Setting system Mail app as handler while current handler is Gmail should
  // trigger the observer callback, incrementing count from 0 to 1.
  [rewriter setDefaultHandlerID:[systemMailHandler appStoreID]];
  EXPECT_EQ(1, [observer changeCount]);

  // Setting system Mail app as handler while current handler is already
  // system Mail app should not trigger the observer callback. Count remains
  // at 1.
  [rewriter setDefaultHandlerID:[systemMailHandler appStoreID]];
  EXPECT_EQ(1, [observer changeCount]);

  // Setting Gmail app as the default handler when current handler is system
  // Mail app should trigger the observer callback. Count increases from 1 to 2.
  [rewriter setDefaultHandlerID:[fakeGmailHandler appStoreID]];
  EXPECT_EQ(2, [observer changeCount]);

  // Deallocates the observer to test that there are no ill side-effects (e.g.
  // crashes) when observer's lifetime is shorter than the rewriter.
  observer = nil;
  [rewriter setDefaultHandlerID:[systemMailHandler appStoreID]];
}

// Tests that a new user without Gmail app installed launches system Mail app.
TEST_F(LegacyMailtoURLRewriterTest, TestNewUserNoGmail) {
  // Sets pre-condition for a user who did not have Chrome installed.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLegacyShouldAutoOpenKey];
  // A faked MailtoHandler for Gmail.
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailNotInstalled alloc] init];

  // Sets up a LegacyMailtoURLRewriter for testing.
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];

  // Verify that LegacyMailtoURLRewriter will use the system Mail app.
  EXPECT_NSEQ([LegacyMailtoURLRewriter systemMailApp],
              [rewriter defaultHandlerID]);
}

// Tests that a new user with Gmail app installed launches Gmail app.
TEST_F(LegacyMailtoURLRewriterTest, TestNewUserWithGmail) {
  // Sets pre-condition for a user who did not have Chrome installed.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLegacyShouldAutoOpenKey];
  // A faked MailtoHandler for Gmail.
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];

  // Sets up a LegacyMailtoURLRewriter for testing.
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];

  // Verify that LegacyMailtoURLRewriter will use Gmail app.
  EXPECT_NSEQ(kGmailAppStoreID, [rewriter defaultHandlerID]);
}

// Tests that a user who has Gmail installed but has chosen not to use Gmail
// as the app to handle mailto: links retains the same behavior when upgrading
// Chrome.
TEST_F(LegacyMailtoURLRewriterTest, TestUpgradeUserWithGmailDisabled) {
  // Sets pre-condition for a user who had Chrome installed.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:@(kAutoOpenLinksNo) forKey:kLegacyShouldAutoOpenKey];
  // A faked MailtoHandler for Gmail.
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];

  // Sets up a LegacyMailtoURLRewriter for testing.
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];

  // Verify that LegacyMailtoURLRewriter will use the system Mail app. As part
  // of the "upgrade", the legacy key should be removed as well.
  EXPECT_NSEQ([LegacyMailtoURLRewriter systemMailApp],
              [rewriter defaultHandlerID]);
  EXPECT_FALSE([defaults objectForKey:kLegacyShouldAutoOpenKey]);
}

// Tests that a user who has Gmail installed and has chosen to use Gmail as the
// app to handle mailto: links retains the same behavior.
TEST_F(LegacyMailtoURLRewriterTest, TestUpgradeUserWithGmailEnabled) {
  // Sets pre-condition for a user who did not have Chrome installed.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:@(kAutoOpenLinksYes) forKey:kLegacyShouldAutoOpenKey];
  // A faked MailtoHandler for Gmail.
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];

  // Sets up a LegacyMailtoURLRewriter for testing.
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];

  // Verify that LegacyMailtoURLRewriter will use Gmail app. As part of the
  // upgrade, the legacy key should be removed as well.
  EXPECT_NSEQ(kGmailAppStoreID, [rewriter defaultHandlerID]);
  EXPECT_FALSE([defaults objectForKey:kLegacyShouldAutoOpenKey]);
}

// Tests that a user who installed Gmail after started using Chrome gets Gmail
// as the handler of mailto: links.
TEST_F(LegacyMailtoURLRewriterTest, TestInstalledGmailAfterChrome) {
  // Pre-condition for a user who did not have Chrome installed.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLegacyShouldAutoOpenKey];
  // A faked MailtoHandler for Gmail.
  MailtoHandler* fakeGmailHandler =
      [[FakeMailtoHandlerGmailInstalled alloc] init];

  // Sets up a LegacyMailtoURLRewriter for testing.
  LegacyMailtoURLRewriter* rewriter = [[LegacyMailtoURLRewriter alloc] init];
  MailtoHandler* systemMailHandler = [[MailtoHandlerSystemMail alloc] init];
  [rewriter setDefaultHandlers:@[ systemMailHandler, fakeGmailHandler ]];

  // Verify that LegacyMailtoURLRewriter will use Gmail app.
  EXPECT_NSEQ(kGmailAppStoreID, [rewriter defaultHandlerID]);
}
