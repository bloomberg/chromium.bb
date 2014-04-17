// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/events/test/cocoa_test_event_utils.h"

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
  base::scoped_nsobject<AutofillTextField> textfield_;

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

// Test multiline behavior.
TEST_F(AutofillTextFieldTest, Multiline) {
  [[textfield_ window] makeFirstResponder:textfield_];

  // First, test with multiline disabled (default state).
  ASSERT_EQ(NO, [textfield_ isMultiline]);

  // All input interactions must happen via the field editor - AutofillTextField
  // is based on NSTextField.
  [[textfield_ currentEditor] insertText:@"foo"];

  // Insert a newline. Must do this via simulated key event to trigger
  // -control:textView:doCommandBySelector:.
  [[textfield_ currentEditor]
      keyDown:cocoa_test_event_utils::KeyEventWithCharacter('\n')];
  [[textfield_ currentEditor] insertText:@"bar"];
  EXPECT_NSEQ(@"bar", [textfield_ stringValue]);

  // Now test with multiline mode enabled.
  [textfield_ setIsMultiline:YES];
  [textfield_ setStringValue:@""];
  [[textfield_ currentEditor] insertText:@"foo"];
  [[textfield_ currentEditor]
      keyDown:cocoa_test_event_utils::KeyEventWithCharacter('\n')];
  [[textfield_ currentEditor] insertText:@"bar"];
  EXPECT_NSEQ(@"foo\nbar", [textfield_ stringValue]);
}
