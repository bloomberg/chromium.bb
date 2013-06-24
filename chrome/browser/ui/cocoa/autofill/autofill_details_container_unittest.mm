// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/autofill/mock_autofill_dialog_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

namespace {

class AutofillDetailsContainerTest : public ui::CocoaTest {
 public:
  AutofillDetailsContainerTest() {
    container_.reset([[AutofillDetailsContainer alloc] initWithController:
                         &controller_]);
    [[test_window() contentView] addSubview:[container_ view]];
  }

 protected:
  base::scoped_nsobject<AutofillDetailsContainer> container_;
  testing::NiceMock<autofill::MockAutofillDialogController> controller_;
};

}  // namespace

TEST_VIEW(AutofillDetailsContainerTest, [container_ view])

TEST_F(AutofillDetailsContainerTest, BasicProperties) {
  EXPECT_GT([[[container_ view] subviews] count], 0U);
  EXPECT_GT(NSHeight([[container_ view] frame]), 0);
  EXPECT_GT(NSWidth([[container_ view] frame]), 0);
}
