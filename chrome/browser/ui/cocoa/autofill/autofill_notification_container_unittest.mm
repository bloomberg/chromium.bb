// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_notification_container.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillNotificationContainerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    container_.reset([[AutofillNotificationContainer alloc] initWithDelegate:
                         &delegate_]);
    [[test_window() contentView] addSubview:[container_ view]];
  }

 protected:
  base::scoped_nsobject<AutofillNotificationContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogViewDelegate> delegate_;
};

}  // namespace

TEST_VIEW(AutofillNotificationContainerTest, [container_ view])

TEST_F(AutofillNotificationContainerTest, Subviews) {
  // No subviews if there are no notifications.
  EXPECT_EQ(0U, [[[container_ view] subviews] count]);

  // Single notification adds one subview.
  std::vector<autofill::DialogNotification> notifications;
  notifications.push_back(
      autofill::DialogNotification(
          autofill::DialogNotification::REQUIRED_ACTION,
          base::ASCIIToUTF16("test")));
  ASSERT_FALSE(notifications[0].HasArrow());
  [container_ setNotifications:notifications];

  EXPECT_EQ(1U, [[[container_ view] subviews] count]);

  // 2 notifications, one with arrow - 2 views.
  notifications.push_back(
      autofill::DialogNotification(
          autofill::DialogNotification::REQUIRED_ACTION,
          base::ASCIIToUTF16("test")));
  ASSERT_FALSE(notifications[1].HasArrow());
  [container_ setNotifications:notifications];

  EXPECT_EQ(2U, [[[container_ view] subviews] count]);

  // Just to exercise code path.
  [container_ performLayout];
}
