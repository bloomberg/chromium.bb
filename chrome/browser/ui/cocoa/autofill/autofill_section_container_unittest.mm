// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"
#import "chrome/browser/ui/cocoa/autofill/layout_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AutofillSectionContainerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    container_.reset(
        [[AutofillSectionContainer alloc]
            initWithController:&controller_
                    forSection:autofill::SECTION_CC]);
    [[test_window() contentView] addSubview:[container_ view]];
  }

 protected:
  scoped_nsobject<AutofillSectionContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogController> controller_;
};

}  // namespace

TEST_VIEW(AutofillSectionContainerTest, [container_ view])

TEST_F(AutofillSectionContainerTest, HasSubviews) {
  ASSERT_EQ(2U, [[[container_ view] subviews] count]);

  bool hasLayoutView = false;
  bool hasTextField = false;
  for (NSView* view in [[container_ view] subviews]) {
    if ([view isKindOfClass:[NSTextField class]]) {
      hasTextField = true;
    } else if ([view isKindOfClass:[LayoutView class]]) {
      hasLayoutView = true;
    }
  }

  EXPECT_TRUE(hasLayoutView);
  EXPECT_TRUE(hasTextField);
}