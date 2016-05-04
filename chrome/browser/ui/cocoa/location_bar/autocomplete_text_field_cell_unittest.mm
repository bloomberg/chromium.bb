// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/keyword_hint_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/selected_keyword_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/star_decoration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/material_design/material_design_controller.h"

using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace {

// Width of the field so that we don't have to ask |field_| for it all
// the time.
const CGFloat kWidth(300.0);

class MockDecoration : public LocationBarDecoration {
 public:
  virtual CGFloat GetWidthForSpace(CGFloat width) { return 20.0; }

  MOCK_METHOD2(DrawInFrame, void(NSRect frame, NSView* control_view));
  MOCK_METHOD0(GetToolTip, NSString*());
};

class AutocompleteTextFieldCellTest : public CocoaTest {
 public:
  AutocompleteTextFieldCellTest() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    const NSRect frame = NSMakeRect(0, 0, kWidth, 30);

    base::scoped_nsobject<NSTextField> view(
        [[NSTextField alloc] initWithFrame:frame]);
    view_ = view.get();

    base::scoped_nsobject<AutocompleteTextFieldCell> cell(
        [[AutocompleteTextFieldCell alloc] initTextCell:@"Testing"]);
    [cell setEditable:YES];
    [cell setBordered:YES];

    [cell clearDecorations];
    mock_left_decoration_.SetVisible(false);
    [cell addLeftDecoration:&mock_left_decoration_];
    mock_right_decoration0_.SetVisible(false);
    mock_right_decoration1_.SetVisible(false);
    [cell addRightDecoration:&mock_right_decoration0_];
    [cell addRightDecoration:&mock_right_decoration1_];

    [view_ setCell:cell.get()];

    [[test_window() contentView] addSubview:view_];
  }

  NSTextField* view_;
  MockDecoration mock_left_decoration_;
  MockDecoration mock_right_decoration0_;
  MockDecoration mock_right_decoration1_;
};

// Basic view tests (AddRemove, Display).
TEST_VIEW(AutocompleteTextFieldCellTest, view_);

// Test drawing, mostly to ensure nothing leaks or crashes.
// Flaky, disabled. Bug http://crbug.com/49522
TEST_F(AutocompleteTextFieldCellTest, DISABLED_FocusedDisplay) {
  [view_ display];

  // Test focused drawing.
  [test_window() makePretendKeyWindowAndSetFirstResponder:view_];
  [view_ display];
  [test_window() clearPretendKeyWindowAndFirstResponder];

  // Test display of various cell configurations.
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  // Load available decorations and try drawing.  To make sure that
  // they are actually drawn, check that |GetWidthForSpace()| doesn't
  // indicate that they should be omitted.
  const CGFloat kVeryWide = 1000.0;

  SelectedKeywordDecoration selected_keyword_decoration;
  selected_keyword_decoration.SetVisible(true);
  selected_keyword_decoration.SetKeyword(base::ASCIIToUTF16("Google"), false);
  [cell addLeftDecoration:&selected_keyword_decoration];
  EXPECT_NE(selected_keyword_decoration.GetWidthForSpace(kVeryWide),
            LocationBarDecoration::kOmittedWidth);

  // TODO(shess): This really wants a |LocationBarViewMac|, but only a
  // few methods reference it, so this works well enough.  But
  // something better would be nice.
  LocationIconDecoration location_icon_decoration(NULL);
  location_icon_decoration.SetVisible(true);
  location_icon_decoration.SetImage([NSImage imageNamed:@"NSApplicationIcon"]);
  [cell addLeftDecoration:&location_icon_decoration];
  EXPECT_NE(location_icon_decoration.GetWidthForSpace(kVeryWide),
            LocationBarDecoration::kOmittedWidth);

  EVBubbleDecoration ev_bubble_decoration(&location_icon_decoration);
  ev_bubble_decoration.SetVisible(true);
  ev_bubble_decoration.SetImage([NSImage imageNamed:@"NSApplicationIcon"]);
  ev_bubble_decoration.SetLabel(@"Application");
  [cell addLeftDecoration:&ev_bubble_decoration];
  EXPECT_NE(ev_bubble_decoration.GetWidthForSpace(kVeryWide),
            LocationBarDecoration::kOmittedWidth);

  StarDecoration star_decoration(NULL);
  star_decoration.SetVisible(true);
  [cell addRightDecoration:&star_decoration];
  EXPECT_NE(star_decoration.GetWidthForSpace(kVeryWide),
            LocationBarDecoration::kOmittedWidth);

  KeywordHintDecoration keyword_hint_decoration;
  keyword_hint_decoration.SetVisible(true);
  keyword_hint_decoration.SetKeyword(base::ASCIIToUTF16("google"), false);
  [cell addRightDecoration:&keyword_hint_decoration];
  EXPECT_NE(keyword_hint_decoration.GetWidthForSpace(kVeryWide),
            LocationBarDecoration::kOmittedWidth);

  // Make sure we're actually calling |DrawInFrame()|.
  StrictMock<MockDecoration> mock_decoration;
  mock_decoration.SetVisible(true);
  [cell addLeftDecoration:&mock_decoration];
  EXPECT_CALL(mock_decoration, DrawInFrame(_, _));
  EXPECT_NE(mock_decoration.GetWidthForSpace(kVeryWide),
            LocationBarDecoration::kOmittedWidth);

  [view_ display];

  [cell clearDecorations];
}

TEST_F(AutocompleteTextFieldCellTest, TextFrame) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds([view_ bounds]);
  NSRect textFrame;

  // The cursor frame should stay the same throughout.
  const NSRect cursorFrame([cell textCursorFrameForFrame:bounds]);
  EXPECT_NSEQ(cursorFrame, bounds);

  // At default settings, everything goes to the text area.
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  if (ui::MaterialDesignController::IsModeMaterial()) {
    EXPECT_EQ(1, NSMinX(textFrame));
  } else {
    EXPECT_EQ(NSMinX(bounds), NSMinX(textFrame));
  }
  EXPECT_EQ(NSMaxX(bounds), NSMaxX(textFrame));
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Decoration on the left takes up space.
  mock_left_decoration_.SetVisible(true);
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_GT(NSMinX(textFrame), NSMinX(bounds));
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));
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
  const NSRect originalDrawingRect = drawingRect;

  mock_left_decoration_.SetVisible(true);
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));

  mock_right_decoration0_.SetVisible(true);
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));

  mock_left_decoration_.SetVisible(false);
  mock_right_decoration0_.SetVisible(false);
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_NSEQ(drawingRect, originalDrawingRect);
}

// Test that left decorations are at the correct edge of the cell.
TEST_F(AutocompleteTextFieldCellTest, LeftDecorationFrame) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds = [view_ bounds];

  mock_left_decoration_.SetVisible(true);
  const NSRect decorationRect =
      [cell frameForDecoration:&mock_left_decoration_ inFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(decorationRect));
  EXPECT_TRUE(NSContainsRect(bounds, decorationRect));

  // Decoration should be left of |drawingRect|.
  const NSRect drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_GT(NSMinX(drawingRect), NSMinX(decorationRect));

  // Decoration should be left of |textFrame|.
  const NSRect textFrame = [cell textFrameForFrame:bounds];
  EXPECT_GT(NSMinX(textFrame), NSMinX(decorationRect));
}

// Test that right decorations are at the correct edge of the cell.
TEST_F(AutocompleteTextFieldCellTest, RightDecorationFrame) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds = [view_ bounds];

  mock_right_decoration0_.SetVisible(true);
  mock_right_decoration1_.SetVisible(true);

  const NSRect decoration0Rect =
      [cell frameForDecoration:&mock_right_decoration0_ inFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(decoration0Rect));
  EXPECT_TRUE(NSContainsRect(bounds, decoration0Rect));

  // Right-side decorations are ordered from rightmost to leftmost.
  // Outer decoration (0) to right of inner decoration (1).
  const NSRect decoration1Rect =
      [cell frameForDecoration:&mock_right_decoration1_ inFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(decoration1Rect));
  EXPECT_TRUE(NSContainsRect(bounds, decoration1Rect));
  EXPECT_LT(NSMinX(decoration1Rect), NSMinX(decoration0Rect));

  // Decoration should be right of |drawingRect|.
  const NSRect drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_LT(NSMinX(drawingRect), NSMinX(decoration1Rect));

  // Decoration should be right of |textFrame|.
  const NSRect textFrame = [cell textFrameForFrame:bounds];
  EXPECT_LT(NSMinX(textFrame), NSMinX(decoration1Rect));
}

// Verify -[AutocompleteTextFieldCell updateToolTipsInRect:ofView:].
TEST_F(AutocompleteTextFieldCellTest, UpdateToolTips) {
  NSString* tooltip = @"tooltip";

  // Left decoration returns a tooltip, make sure it is called at
  // least once.
  mock_left_decoration_.SetVisible(true);
  EXPECT_CALL(mock_left_decoration_, GetToolTip())
      .WillOnce(Return(tooltip))
      .WillRepeatedly(Return(tooltip));

  // Right decoration returns no tooltip, make sure it is called at
  // least once.
  mock_right_decoration0_.SetVisible(true);
  EXPECT_CALL(mock_right_decoration0_, GetToolTip())
      .WillOnce(Return((NSString*)nil))
      .WillRepeatedly(Return((NSString*)nil));

  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds = [view_ bounds];
  const NSRect leftDecorationRect =
      [cell frameForDecoration:&mock_left_decoration_ inFrame:bounds];

  // |controlView| gets the tooltip for the left decoration.
  id controlView = [OCMockObject mockForClass:[AutocompleteTextField class]];
  [[controlView expect] addToolTip:tooltip forRect:leftDecorationRect];

  [cell updateToolTipsInRect:bounds ofView:controlView];

  EXPECT_OCMOCK_VERIFY(controlView);
}

}  // namespace
