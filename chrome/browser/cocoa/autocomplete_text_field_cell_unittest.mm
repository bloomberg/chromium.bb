// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class AutocompleteTextFieldCellTest : public PlatformTest {
 public:
  AutocompleteTextFieldCellTest() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    NSRect frame = NSMakeRect(0, 0, 300, 30);
    view_.reset([[NSTextField alloc] initWithFrame:frame]);
    scoped_nsobject<AutocompleteTextFieldCell> cell(
        [[AutocompleteTextFieldCell alloc] initTextCell:@"Testing"]);
    [view_ setCell:cell.get()];
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSTextField> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(AutocompleteTextFieldCellTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldCellTest, Display) {
  [view_ display];

  // Test display of various cell configurations.
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  [cell setSearchHintString:@"Type to search"];
  [view_ display];

  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"prefix" image:image suffix:@"suffix"];
  [view_ display];

  [cell setKeywordString:@"Search Engine:"];
  [view_ display];
}

TEST_F(AutocompleteTextFieldCellTest, SearchHint) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  // At default settings, everything is already good to go.
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Setting a search hint will need a reset.
  [cell setSearchHintString:@"Type to search"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Changing the search hint needs a reset.
  [cell setSearchHintString:@"Type to find"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Changing to an identical string doesn't need a reset.
  [cell setSearchHintString:@"Type to find"];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
}

TEST_F(AutocompleteTextFieldCellTest, KeywordHint) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];

  // At default settings, everything is already good to go.
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Setting a keyword hint will need a reset.
  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Changing the keyword hint needs a reset.
  [cell setKeywordHintPrefix:@"Type " image:image suffix:@" to search Engine"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Changing to identical strings doesn't need a reset.
  [cell setKeywordHintPrefix:@"Type " image:image suffix:@" to search Engine"];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
}

TEST_F(AutocompleteTextFieldCellTest, KeywordString) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  // At default settings, everything is already good to go.
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Setting a keyword string will need a reset.
  [cell setKeywordString:@"Search Engine:"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Changing the keyword string needs a reset.
  [cell setKeywordString:@"Search on Engine:"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);

  // Changing to an identical string doesn't need a reset.
  [cell setKeywordString:@"Search on Engine:"];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
}

// Test that transitions between various modes set the reset flag.
TEST_F(AutocompleteTextFieldCellTest, Transitions) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];

  // Transitions from hint to keyword string, keyword hint, and
  // cleared.
  [cell setSearchHintString:@"Type to search"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell setKeywordString:@"Search Engine:"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  [cell setSearchHintString:@"Type to search"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  [cell setSearchHintString:@"Type to search"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  // Transitions from keyword hint to keyword string, simple hint, and
  // cleared.
  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell setSearchHintString:@"Type to search"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell setKeywordString:@"Search Engine:"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  // Transitions from keyword string to either type of hint, or
  // cleared.
  [cell setKeywordString:@"Search on Engine:"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell setSearchHintString:@"Type to search"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  [cell setKeywordString:@"Search on Engine:"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);

  [cell setKeywordString:@"Search on Engine:"];
  [cell setFieldEditorNeedsReset:NO];
  EXPECT_FALSE([cell fieldEditorNeedsReset]);
  [cell clearKeywordAndHint];
  EXPECT_TRUE([cell fieldEditorNeedsReset]);
}

TEST_F(AutocompleteTextFieldCellTest, TextFrame) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds([view_ bounds]);
  NSRect textFrame;

  // The cursor frame should stay the same throughout.
  const NSRect cursorFrame([cell textCursorFrameForFrame:bounds]);

  // At default settings, everything goes to the text area.
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_EQ(NSMinX(bounds), NSMinX(textFrame));
  EXPECT_EQ(NSMaxX(bounds), NSMaxX(textFrame));
  EXPECT_TRUE(NSEqualRects(cursorFrame, textFrame));

  // Small search hint leaves text frame to left.
  [cell setSearchHintString:@"Search hint"];
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_LT(NSMaxX(textFrame), NSMaxX(bounds));
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Save search-hint's frame for future reference.
  const CGFloat searchHintMaxX(NSMaxX(textFrame));

  // Keyword hint also leaves text to left.  Arrange for it to be
  // larger than the search hint just to make sure that things have
  // changed (keyword hint overrode search hint), and that they aren't
  // handled identically w/out reference to the actual button cell.
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"Keyword " image:image suffix:@" hint"];
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_LT(NSMaxX(textFrame), NSMaxX(bounds));
  EXPECT_LT(NSMaxX(textFrame), searchHintMaxX);
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Keyword search leaves text area to right.
  [cell setKeywordString:@"Search Engine:"];
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_GT(NSMinX(textFrame), NSMinX(bounds));
  EXPECT_LT(NSMinX(textFrame), searchHintMaxX);
  EXPECT_GT(NSMaxX(textFrame), searchHintMaxX);
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Text frame should take everything over again on reset.
  [cell clearKeywordAndHint];
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_EQ(NSMinX(bounds), NSMinX(textFrame));
  EXPECT_EQ(NSMaxX(bounds), NSMaxX(textFrame));
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));
}

}  // namespace
