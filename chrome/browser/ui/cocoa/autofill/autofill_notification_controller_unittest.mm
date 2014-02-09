// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "testing/gtest_mac.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

using base::ASCIIToUTF16;

namespace {

class AutofillNotificationControllerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    InitControllerWithNotification(
        autofill::DialogNotification(autofill::DialogNotification::NONE,
                                     ASCIIToUTF16("A notification title")));
  }

  void InitControllerWithNotification(
      const autofill::DialogNotification& notification) {
    controller_.reset(
        [[AutofillNotificationController alloc]
             initWithNotification:&notification
                         delegate:NULL]);
    [[test_window() contentView] setSubviews:@[[controller_ view]]];
  }

 protected:
  base::scoped_nsobject<AutofillNotificationController> controller_;
};

}  // namespace

TEST_VIEW(AutofillNotificationControllerTest, [controller_ view])

TEST_F(AutofillNotificationControllerTest, Subviews) {
  NSView* view = [controller_ view];
  ASSERT_EQ(3U, [[view subviews] count]);
  EXPECT_TRUE([[[view subviews] objectAtIndex:0] isKindOfClass:
      [NSTextView class]]);
  EXPECT_TRUE([[[view subviews] objectAtIndex:1] isKindOfClass:
      [NSButton class]]);
  EXPECT_TRUE([[[view subviews] objectAtIndex:2] isKindOfClass:
      [NSButton class]]);
  EXPECT_NSEQ([controller_ textview],
              [[view subviews] objectAtIndex:0]);
  EXPECT_NSEQ([controller_ checkbox],
              [[view subviews] objectAtIndex:1]);
  EXPECT_NSEQ([controller_ tooltipView],
              [[view subviews] objectAtIndex:2]);

  // Just to exercise the code path.
  [controller_ performLayout];
}

TEST_F(AutofillNotificationControllerTest, TextLabelOnly) {
  InitControllerWithNotification(
      autofill::DialogNotification(
          autofill::DialogNotification::DEVELOPER_WARNING,
          ASCIIToUTF16("A notification title")));

  EXPECT_FALSE([[controller_ textview] isHidden]);
  EXPECT_TRUE([[controller_ checkbox] isHidden]);
  EXPECT_TRUE([[controller_ tooltipView] isHidden]);
}

TEST_F(AutofillNotificationControllerTest, CheckboxOnly) {
  autofill::DialogNotification notification(
          autofill::DialogNotification::WALLET_USAGE_CONFIRMATION,
          ASCIIToUTF16("A notification title"));
  ASSERT_TRUE(notification.HasCheckbox());
  InitControllerWithNotification(notification);

  EXPECT_TRUE([[controller_ textview] isHidden]);
  EXPECT_FALSE([[controller_ checkbox] isHidden]);
  EXPECT_TRUE([[controller_ tooltipView] isHidden]);
}

TEST_F(AutofillNotificationControllerTest, TextLabelAndTooltip) {
  autofill::DialogNotification notification(
          autofill::DialogNotification::DEVELOPER_WARNING,
          ASCIIToUTF16("A notification title"));
  notification.set_tooltip_text(ASCIIToUTF16("My very informative tooltip."));
  InitControllerWithNotification(notification);

  EXPECT_FALSE([[controller_ textview] isHidden]);
  EXPECT_TRUE([[controller_ checkbox] isHidden]);
  EXPECT_FALSE([[controller_ tooltipView] isHidden]);
}

TEST_F(AutofillNotificationControllerTest, CheckboxAndTooltip) {
  autofill::DialogNotification notification(
          autofill::DialogNotification::WALLET_USAGE_CONFIRMATION,
          ASCIIToUTF16("A notification title"));
  ASSERT_TRUE(notification.HasCheckbox());
  notification.set_tooltip_text(ASCIIToUTF16("My very informative tooltip."));
  InitControllerWithNotification(notification);

  EXPECT_TRUE([[controller_ textview] isHidden]);
  EXPECT_FALSE([[controller_ checkbox] isHidden]);
  EXPECT_FALSE([[controller_ tooltipView] isHidden]);
}
