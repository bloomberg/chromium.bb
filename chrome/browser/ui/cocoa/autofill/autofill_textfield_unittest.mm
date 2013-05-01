// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class AutofillTextFieldTest : public CocoaTest {
 public:
  AutofillTextFieldTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 30);
    textfield_.reset([[AutofillTextField alloc] initWithFrame:frame]);
    [textfield_ setStringValue:@"Abcdefg"];
    [textfield_ sizeToFit];
    [[test_window() contentView] addSubview:textfield_];
  }

 protected:
  scoped_nsobject<AutofillTextField> textfield_;

  DISALLOW_COPY_AND_ASSIGN(AutofillTextFieldTest);
};

TEST_VIEW(AutofillTextFieldTest, textfield_)

// Test invalid, mostly to ensure nothing leaks or crashes.
TEST_F(AutofillTextFieldTest, DisplayWithInvalid) {
  [[textfield_ cell] setInvalid:YES];
  [textfield_ display];
  [[textfield_ cell] setInvalid:NO];
  [textfield_ display];
}

// Test with icon, mostly to ensure nothing leaks or crashes.
TEST_F(AutofillTextFieldTest, DisplayWithIcon) {
  NSImage* image = [NSImage imageNamed:NSImageNameStatusAvailable];
  [[textfield_ cell] setIcon:image];
  [textfield_ sizeToFit];
  [textfield_ display];
  [[textfield_ cell] setIcon:nil];
  [textfield_ sizeToFit];
  [textfield_ display];
}
