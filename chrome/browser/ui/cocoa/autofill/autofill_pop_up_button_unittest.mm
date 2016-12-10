// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_pop_up_button.h"

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class AutofillPopUpButtonTest : public CocoaTest {
 public:
  AutofillPopUpButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    button_.reset([[AutofillPopUpButton alloc] initWithFrame:frame]);
    [button_ sizeToFit];
    [[test_window() contentView] addSubview:button_];
  }

 protected:
  base::scoped_nsobject<AutofillPopUpButton> button_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopUpButtonTest);
};

TEST_VIEW(AutofillPopUpButtonTest, button_)

// Test invalid, mostly to ensure nothing leaks or crashes.
TEST_F(AutofillPopUpButtonTest, DisplayWithInvalid) {
  [button_ setValidityMessage:nil];
  [button_ display];
  [button_ setValidityMessage:@"Something is rotten in the state of Denmark"];
  [button_ display];
}
