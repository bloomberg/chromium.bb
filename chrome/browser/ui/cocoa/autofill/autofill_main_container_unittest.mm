// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_view_delegate.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillMainContainerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    RebuildView();
  }

  void RebuildView() {
    container_.reset([[AutofillMainContainer alloc] initWithDelegate:
                         &delegate_]);
    [[test_window() contentView] setSubviews:@[ [container_ view] ]];
  }

 protected:
  base::scoped_nsobject<AutofillMainContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogViewDelegate> delegate_;
};

}  // namespace

TEST_VIEW(AutofillMainContainerTest, [container_ view])

TEST_F(AutofillMainContainerTest, SubViews) {
  bool hasButtons = false;
  bool hasButtonStripImage = false;
  bool hasTextView = false;
  bool hasDetailsContainer = false;
  bool hasCheckbox = false;
  bool hasCheckboxTooltip = false;
  bool hasNotificationContainer = false;

  // Should have account chooser, button strip, and details section.
  EXPECT_EQ(7U, [[[container_ view] subviews] count]);
  for (NSView* view in [[container_ view] subviews]) {
    NSArray* subviews = [view subviews];
    if ([view isKindOfClass:[NSScrollView class]]) {
      hasDetailsContainer = true;
    } else if (view == [container_ saveInChromeTooltipForTesting]) {
      hasCheckboxTooltip = true;
    } else if ([subviews count] == 2) {
      EXPECT_TRUE(
          [[subviews objectAtIndex:0] isKindOfClass:[NSButton class]]);
      EXPECT_TRUE(
          [[subviews objectAtIndex:1] isKindOfClass:[NSButton class]]);
      hasButtons = true;
    } else if ([view isKindOfClass:[NSImageView class]]) {
      if (view == [container_ buttonStripImageForTesting])
        hasButtonStripImage = true;
      else
        EXPECT_TRUE(false);  // Unknown image view; should not be reachable.
    } else if ([view isKindOfClass:[NSTextView class]]) {
      hasTextView = true;
    } else if ([view isKindOfClass:[NSButton class]] &&
               view == [container_ saveInChromeCheckboxForTesting]) {
      hasCheckbox = true;
    } else {
      // Assume by default this is the notification area view.
      // There is no way to easily verify that with more detail.
      hasNotificationContainer = true;
    }
  }

  EXPECT_TRUE(hasButtons);
  EXPECT_TRUE(hasButtonStripImage);
  EXPECT_TRUE(hasTextView);
  EXPECT_TRUE(hasDetailsContainer);
  EXPECT_TRUE(hasNotificationContainer);
  EXPECT_TRUE(hasCheckbox);
  EXPECT_TRUE(hasCheckboxTooltip);
}

// Ensure the default state of the "Save in Chrome" checkbox is controlled by
// the delegate.
TEST_F(AutofillMainContainerTest, SaveDetailsDefaultsFromDelegate) {
  using namespace testing;
  EXPECT_CALL(delegate_, ShouldOfferToSaveInChrome()).Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_,ShouldSaveInChrome()).Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  RebuildView();
  EXPECT_FALSE([container_ saveDetailsLocally]);

  RebuildView();
  EXPECT_TRUE([container_ saveDetailsLocally]);
}

TEST_F(AutofillMainContainerTest, SaveInChromeCheckboxVisibility) {
  using namespace testing;

  // Tests that the checkbox is only visible if the delegate allows it.
  EXPECT_CALL(delegate_, ShouldOfferToSaveInChrome()).Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  RebuildView();
  NSButton* checkbox = [container_ saveInChromeCheckboxForTesting];
  NSImageView* tooltip = [container_ saveInChromeTooltipForTesting];
  ASSERT_TRUE(checkbox);
  ASSERT_TRUE(tooltip);

  EXPECT_TRUE([checkbox isHidden]);
  EXPECT_TRUE([tooltip isHidden]);
  [container_ modelChanged];
  EXPECT_FALSE([checkbox isHidden]);
  EXPECT_FALSE([tooltip isHidden]);
}
