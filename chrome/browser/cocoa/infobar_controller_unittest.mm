// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/infobar_container_controller.h"
#import "chrome/browser/cocoa/infobar_controller.h"
#include "chrome/browser/cocoa/infobar_test_helper.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface InfoBarController (ExposedForTesting)
- (NSTextField*)label;
@end

@implementation InfoBarController (ExposedForTesting)
- (NSTextField*)label {
  return label_;
}
@end


// Calls to removeDelegate: normally start an animation, which removes the
// infobar completely when finished.  For unittesting purposes, we create a mock
// container which calls close: immediately, rather than kicking off an
// animation.
@interface InfoBarContainerTest : NSObject <InfoBarContainer> {
  InfoBarController* controller_;
}
- (id)initWithController:(InfoBarController*)controller;
- (void)removeDelegate:(InfoBarDelegate*)delegate;
- (void)removeController:(InfoBarController*)controller;
@end

@implementation InfoBarContainerTest
- (id)initWithController:(InfoBarController*)controller {
  if ((self = [super init])) {
    controller_ = controller;
  }
  return self;
}

- (void)removeDelegate:(InfoBarDelegate*)delegate {
  [controller_ close];
}

- (void)removeController:(InfoBarController*)controller {
  DCHECK(controller_ == controller);
  controller_ = nil;
}
@end

namespace {

///////////////////////////////////////////////////////////////////////////
// Test fixtures

class AlertInfoBarControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    controller_.reset(
        [[AlertInfoBarController alloc] initWithDelegate:&delegate_]);
    container_.reset(
        [[InfoBarContainerTest alloc] initWithController:controller_]);
    [controller_ setContainerController:container_];
    [[test_window() contentView] addSubview:[controller_ view]];
  }

 protected:
  MockAlertInfoBarDelegate delegate_;
  scoped_nsobject<id> container_;
  scoped_nsobject<AlertInfoBarController> controller_;
};

class LinkInfoBarControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    controller_.reset(
        [[LinkInfoBarController alloc] initWithDelegate:&delegate_]);
    container_.reset(
        [[InfoBarContainerTest alloc] initWithController:controller_]);
    [controller_ setContainerController:container_];
    [[test_window() contentView] addSubview:[controller_ view]];
  }

 protected:
  MockLinkInfoBarDelegate delegate_;
  scoped_nsobject<id> container_;
  scoped_nsobject<LinkInfoBarController> controller_;
};

class ConfirmInfoBarControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();

    controller_.reset(
        [[ConfirmInfoBarController alloc] initWithDelegate:&delegate_]);
    container_.reset(
        [[InfoBarContainerTest alloc] initWithController:controller_]);
    [controller_ setContainerController:container_];
    [[test_window() contentView] addSubview:[controller_ view]];
  }

 protected:
  MockConfirmInfoBarDelegate delegate_;
  scoped_nsobject<id> container_;
  scoped_nsobject<ConfirmInfoBarController> controller_;
};


////////////////////////////////////////////////////////////////////////////
// Tests

TEST_VIEW(AlertInfoBarControllerTest, [controller_ view]);

TEST_F(AlertInfoBarControllerTest, ShowAndDismiss) {
  // Make sure someone looked at the message and icon.
  EXPECT_TRUE(delegate_.message_text_accessed);
  EXPECT_TRUE(delegate_.icon_accessed);

  // Check to make sure the infobar message was set properly.
  EXPECT_EQ(std::wstring(kMockAlertInfoBarMessage),
            base::SysNSStringToWide([[controller_.get() label] stringValue]));

  // Check that dismissing the infobar calls InfoBarClosed() on the delegate.
  [controller_ dismiss:nil];
  EXPECT_TRUE(delegate_.closed);
}

TEST_F(AlertInfoBarControllerTest, DeallocController) {
  // Test that dealloc'ing the controller does not send an
  // InfoBarClosed() message to the delegate.
  controller_.reset(nil);
  EXPECT_FALSE(delegate_.closed);
}

TEST_VIEW(LinkInfoBarControllerTest, [controller_ view]);

TEST_F(LinkInfoBarControllerTest, ShowAndDismiss) {
  // Make sure someone looked at the message, link, and icon.
  EXPECT_TRUE(delegate_.message_text_accessed);
  EXPECT_TRUE(delegate_.link_text_accessed);
  EXPECT_TRUE(delegate_.icon_accessed);

  // Check that dismissing the infobar calls InfoBarClosed() on the delegate.
  [controller_ dismiss:nil];
  EXPECT_FALSE(delegate_.link_clicked);
  EXPECT_TRUE(delegate_.closed);
}

TEST_F(LinkInfoBarControllerTest, ShowAndClickLink) {
  // Check that clicking on the link calls LinkClicked() on the
  // delegate.  It should also close the infobar.
  [controller_ linkClicked];
  EXPECT_TRUE(delegate_.link_clicked);
  EXPECT_TRUE(delegate_.closed);
}

TEST_F(LinkInfoBarControllerTest, ShowAndClickLinkWithoutClosing) {
  delegate_.closes_on_action = false;

  // Check that clicking on the link calls LinkClicked() on the
  // delegate.  It should not close the infobar.
  [controller_ linkClicked];
  EXPECT_TRUE(delegate_.link_clicked);
  EXPECT_FALSE(delegate_.closed);
}

TEST_VIEW(ConfirmInfoBarControllerTest, [controller_ view]);

TEST_F(ConfirmInfoBarControllerTest, ShowAndDismiss) {
  // Make sure someone looked at the message and icon.
  EXPECT_TRUE(delegate_.message_text_accessed);
  EXPECT_TRUE(delegate_.icon_accessed);

  // Check to make sure the infobar message was set properly.
  EXPECT_EQ(std::wstring(kMockConfirmInfoBarMessage),
            base::SysNSStringToWide([[controller_.get() label] stringValue]));

  // Check that dismissing the infobar calls InfoBarClosed() on the delegate.
  [controller_ dismiss:nil];
  EXPECT_FALSE(delegate_.ok_clicked);
  EXPECT_FALSE(delegate_.cancel_clicked);
  EXPECT_TRUE(delegate_.closed);
}

TEST_F(ConfirmInfoBarControllerTest, ShowAndClickOK) {
  // Check that clicking the OK button calls Accept() and then closes
  // the infobar.
  [controller_ ok:nil];
  EXPECT_TRUE(delegate_.ok_clicked);
  EXPECT_FALSE(delegate_.cancel_clicked);
  EXPECT_TRUE(delegate_.closed);
}

TEST_F(ConfirmInfoBarControllerTest, ShowAndClickOKWithoutClosing) {
  delegate_.closes_on_action = false;

  // Check that clicking the OK button calls Accept() but does not close
  // the infobar.
  [controller_ ok:nil];
  EXPECT_TRUE(delegate_.ok_clicked);
  EXPECT_FALSE(delegate_.cancel_clicked);
  EXPECT_FALSE(delegate_.closed);
}

TEST_F(ConfirmInfoBarControllerTest, ShowAndClickCancel) {
  // Check that clicking the cancel button calls Cancel() and closes
  // the infobar.
  [controller_ cancel:nil];
  EXPECT_FALSE(delegate_.ok_clicked);
  EXPECT_TRUE(delegate_.cancel_clicked);
  EXPECT_TRUE(delegate_.closed);
}

TEST_F(ConfirmInfoBarControllerTest, ShowAndClickCancelWithoutClosing) {
  delegate_.closes_on_action = false;

  // Check that clicking the cancel button calls Cancel() but does not close
  // the infobar.
  [controller_ cancel:nil];
  EXPECT_FALSE(delegate_.ok_clicked);
  EXPECT_TRUE(delegate_.cancel_clicked);
  EXPECT_FALSE(delegate_.closed);
}

}  // namespace
