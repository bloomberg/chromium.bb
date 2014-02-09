// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_suggestion_container.h"

#import "chrome/browser/ui/cocoa/autofill/autofill_textfield.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillSuggestionContainerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    container_.reset([[AutofillSuggestionContainer alloc] init]);
    [[test_window() contentView] addSubview:[container_ view]];
    view_ = [container_ view];
  }

 protected:
  base::scoped_nsobject<AutofillSuggestionContainer> container_;
  NSView* view_;
};

}  // namespace

TEST_VIEW(AutofillSuggestionContainerTest, [container_ view])

TEST_F(AutofillSuggestionContainerTest, HasSubviews) {
  ASSERT_EQ(3U, [[[container_ view] subviews] count]);

  bool has_text_view = false;
  bool has_edit_field = false;

  // Ignore |spacer_| NSBox in this test.
  for (id view in [[container_ view] subviews]) {
    if ([view isKindOfClass:[AutofillTextField class]]) {
      has_edit_field = true;
    } else if ([view isKindOfClass:[NSTextView class]]) {
      has_text_view = true;
    }
  }

  EXPECT_TRUE(has_text_view);
  EXPECT_TRUE(has_edit_field);
}

// Test that mouse events outside the input field are ignored.
TEST_F(AutofillSuggestionContainerTest, HitTestInputField) {
 base::scoped_nsobject<NSImage> icon(
      [[NSImage alloc] initWithSize:NSMakeSize(16, 16)]);
  [container_ setSuggestionWithVerticallyCompactText:@"suggest"
                             horizontallyCompactText:@"suggest"
                                                icon:nil
                                            maxWidth:200];
  [container_ showInputField:@"input" withIcon:icon];
  [view_ setFrameSize:[container_ preferredSize]];
  [container_ performLayout];

  // Point not touching any subviews, in |view_|'s coordinate system.
  NSPoint point_outside_subviews =
      NSMakePoint(NSMinX([view_ bounds]), NSMaxY([view_ bounds]) - 1);
  NSPoint point_inside_text_view = NSZeroPoint;

  // hitTests on all inputs should be false, except for the inputField.
  for (id field_view in [view_ subviews]) {
    // Ensure |point_outside_subviews| really is outside subviews.
    ASSERT_FALSE([field_view hitTest:point_outside_subviews]);

    // Compute center of |field_view| in |view_|'s parent coordinate system.
    NSPoint point =
        [view_ convertPoint:NSMakePoint(NSMidX([field_view frame]),
                                        NSMidY([field_view frame]))
                     toView:[view_ superview]];
    if (field_view == [container_ inputField]) {
      point_inside_text_view = point;
      EXPECT_TRUE([view_ hitTest:point]);
    } else {
      EXPECT_FALSE([view_ hitTest:point]);
    }
  }

  // Mouse events directly on the main view should be ignored.
  EXPECT_FALSE([view_ hitTest:
      [view_ convertPoint:point_outside_subviews
                   toView:[view_ superview]]]);

  // Mouse events on the text view's editor should propagate.
  // Asserts ensure the path covering children of |inputField| is taken.
  [[view_ window] makeFirstResponder:[container_ inputField]];
  NSView* editorView = [view_ hitTest:point_inside_text_view];
  ASSERT_NE(editorView, [container_ inputField]);
  ASSERT_TRUE([editorView isDescendantOf:[container_ inputField]]);
  EXPECT_TRUE(editorView);
}
