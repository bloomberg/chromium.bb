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

// Width of the field so that we don't have to ask |field_| for it all
// the time.
const CGFloat kWidth(300.0);

// A narrow width for tests which test things that don't fit.
const CGFloat kNarrowWidth(5.0);

class AutocompleteTextFieldCellTest : public PlatformTest {
 public:
  AutocompleteTextFieldCellTest() : security_image_view_(NULL, NULL) {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    const NSRect frame = NSMakeRect(0, 0, kWidth, 30);
    view_.reset([[NSTextField alloc] initWithFrame:frame]);
    scoped_nsobject<AutocompleteTextFieldCell> cell(
        [[AutocompleteTextFieldCell alloc] initTextCell:@"Testing"]);
    [cell setEditable:YES];
    [cell setBordered:YES];
    [view_ setCell:cell.get()];
    [cell setSecurityImageView:&security_image_view_];
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<NSTextField> view_;
  LocationBarViewMac::SecurityImageView security_image_view_;
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

  // Test focussed drawing.
  cocoa_helper_.makeFirstResponder(view_);
  [view_ display];
  cocoa_helper_.clearFirstResponder();

  // Test display of various cell configurations.
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  [view_ display];

  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"prefix" image:image suffix:@"suffix"
              availableWidth:kWidth];
  [view_ display];

  [cell setKeywordString:@"Search Engine:"
           partialString:@"Search Eng:"
          availableWidth:kWidth];
  [view_ display];
}

// Verify that transitions between states clear other states.
TEST_F(AutocompleteTextFieldCellTest, StateTransitionsResetOthers) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];

  // Setting hint leaves keyword empty.
  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  EXPECT_FALSE([cell keywordString]);

  // Setting keyword clears hint.
  [cell setKeywordString:@"Search Engine:"
           partialString:@"Search Eng:"
          availableWidth:kWidth];
  EXPECT_FALSE([cell hintString]);
  EXPECT_TRUE([cell keywordString]);

  // Setting hint clears keyword.
  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"
              availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  EXPECT_FALSE([cell keywordString]);

  // Clear clears keyword.
  [cell clearKeywordAndHint];
  EXPECT_FALSE([cell hintString]);
  EXPECT_FALSE([cell keywordString]);

  // Clear clears hint.
  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  [cell clearKeywordAndHint];
  EXPECT_FALSE([cell hintString]);
  EXPECT_FALSE([cell keywordString]);
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
  [cell setSearchHintString:@"Search hint" availableWidth:kWidth];
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
  [cell setKeywordHintPrefix:@"Keyword " image:image suffix:@" hint"
              availableWidth:kWidth];
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_LT(NSMaxX(textFrame), NSMaxX(bounds));
  EXPECT_LT(NSMaxX(textFrame), searchHintMaxX);
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Keyword search leaves text area to right.
  [cell setKeywordString:@"Search Engine:"
           partialString:@"Search Eng:"
          availableWidth:kWidth];
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

  // Security icon takes up space on the right
  security_image_view_.SetImageShown(
      LocationBarViewMac::SecurityImageView::LOCK);
  security_image_view_.SetVisible(true);

  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_LT(NSMaxX(textFrame), NSMaxX(bounds));
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Search hint text takes precedence over the hint icon; the text frame
  // should be smaller in order to accomodate the text that is wider than
  // the icon.
  [cell setSearchHintString:@"Search hint" availableWidth:kWidth];
  NSRect textFrameWithHintText = [cell textFrameForFrame:bounds];
  EXPECT_TRUE(NSContainsRect(textFrame, textFrameWithHintText));
  EXPECT_LT(NSWidth(textFrameWithHintText), NSWidth(textFrame));
}

// The editor frame should be slightly inset from the text frame.
TEST_F(AutocompleteTextFieldCellTest, DrawingRectForBounds) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds([view_ bounds]);
  NSRect textFrame, drawingRect;

  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(textFrame, NSInsetRect(drawingRect, 1, 1)));

  // Save the starting frame for after clear.
  const NSRect originalDrawingRect(drawingRect);

  // TODO(shess): Do we really need to test every combination?

  [cell setSearchHintString:@"Search hint" availableWidth:kWidth];
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(textFrame, NSInsetRect(drawingRect, 1, 1)));

  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"Keyword " image:image suffix:@" hint"
              availableWidth:kWidth];
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));

  [cell setKeywordString:@"Search Engine:"
           partialString:@"Search Eng:"
          availableWidth:kWidth];
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));

  [cell clearKeywordAndHint];
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));
  EXPECT_TRUE(NSEqualRects(drawingRect, originalDrawingRect));

  security_image_view_.SetImageShown(
      LocationBarViewMac::SecurityImageView::LOCK);
  security_image_view_.SetVisible(true);

  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));
}

// Test that the security icon is at the right side of the cell.
TEST_F(AutocompleteTextFieldCellTest, HintImageFrame) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds([view_ bounds]);
  security_image_view_.SetImageShown(
      LocationBarViewMac::SecurityImageView::LOCK);

  security_image_view_.SetVisible(false);
  NSRect iconRect = [cell securityImageFrameForFrame:bounds];
  EXPECT_TRUE(NSIsEmptyRect(iconRect));

  // Save the starting frame for after clear.
  const NSRect originalIconRect(iconRect);

  security_image_view_.SetVisible(true);
  iconRect = [cell securityImageFrameForFrame:bounds];

  EXPECT_FALSE(NSIsEmptyRect(iconRect));
  EXPECT_TRUE(NSContainsRect(bounds, iconRect));

  // Make sure we are right of the |drawingRect|.
  NSRect drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_LE(NSMaxX(drawingRect), NSMinX(iconRect));

  // Make sure we're right of the |textFrame|.
  NSRect textFrame = [cell textFrameForFrame:bounds];
  EXPECT_LE(NSMaxX(textFrame), NSMinX(iconRect));

  // Now add a label.
  NSFont* font = [NSFont controlContentFontOfSize:12.0];
  NSColor* color = [NSColor blackColor];
  security_image_view_.SetLabel(@"Label", font, color);
  iconRect = [cell securityImageFrameForFrame:bounds];

  EXPECT_FALSE(NSIsEmptyRect(iconRect));
  EXPECT_TRUE(NSContainsRect(bounds, iconRect));

  // Make sure we are right of the |drawingRect|.
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_LE(NSMaxX(drawingRect), NSMinX(iconRect));

  // Make sure we're right of the |textFrame|.
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_LE(NSMaxX(textFrame), NSMinX(iconRect));

  // Make sure we clear correctly.
  security_image_view_.SetVisible(false);
  iconRect = [cell securityImageFrameForFrame:bounds];
  EXPECT_TRUE(NSEqualRects(iconRect, originalIconRect));
  EXPECT_TRUE(NSIsEmptyRect(iconRect));
}

// Test that the cell correctly chooses the partial keyword if there's
// no enough room.
TEST_F(AutocompleteTextFieldCellTest, UsesPartialKeywordIfNarrow) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  const NSString* kFullString = @"Search Engine:";
  const NSString* kPartialString = @"Search Eng:";

  // Wide width chooses the full string.
  [cell setKeywordString:kFullString
           partialString:kPartialString
          availableWidth:kWidth];
  EXPECT_TRUE([cell keywordString]);
  EXPECT_TRUE([[[cell keywordString] string] isEqualToString:kFullString]);

  // Narrow width chooses the partial string.
  [cell setKeywordString:kFullString
           partialString:kPartialString
          availableWidth:kNarrowWidth];
  EXPECT_TRUE([cell keywordString]);
  EXPECT_TRUE([[[cell keywordString] string] isEqualToString:kPartialString]);

  // But not if there isn't one!
  [cell setKeywordString:kFullString
           partialString:nil
          availableWidth:kNarrowWidth];
  EXPECT_TRUE([cell keywordString]);
  EXPECT_TRUE([[[cell keywordString] string] isEqualToString:kFullString]);
}

// Test that the cell drops the search hint if there is no room for
// it.
TEST_F(AutocompleteTextFieldCellTest, OmitsSearchHintIfNarrow) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  const NSString* kSearchHint = @"Type to search";

  // Wide width chooses the full string.
  [cell setSearchHintString:kSearchHint availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  EXPECT_TRUE([[[cell hintString] string] isEqualToString:kSearchHint]);

  // Narrow width suppresses entirely.
  [cell setSearchHintString:kSearchHint availableWidth:kNarrowWidth];
  EXPECT_FALSE([cell hintString]);
}

// Test that the cell drops all but the image if there is no room for
// the entire keyword hint.
TEST_F(AutocompleteTextFieldCellTest, TrimsKeywordHintIfNarrow) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  scoped_nsobject<NSImage> image(
      [[NSImage alloc] initWithSize:NSMakeSize(20, 20)]);

  const NSString* kHintPrefix = @"Press ";
  const NSString* kHintSuffix = @" to search Engine";

  // Wide width chooses the full string.
  [cell setKeywordHintPrefix:kHintPrefix image:image suffix:kHintSuffix
              availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  EXPECT_TRUE([[[cell hintString] string] hasPrefix:kHintPrefix]);
  EXPECT_TRUE([[[cell hintString] string] hasSuffix:kHintSuffix]);

  // Narrow width suppresses everything but the image.
  [cell setKeywordHintPrefix:kHintPrefix image:image suffix:kHintSuffix
              availableWidth:kNarrowWidth];
  EXPECT_EQ([[cell hintString] length], 1U);
}

}  // namespace
