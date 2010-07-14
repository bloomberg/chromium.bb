// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/resource_bundle.h"
#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/location_bar/autocomplete_text_field_cell.h"
#import "chrome/browser/cocoa/location_bar/ev_bubble_decoration.h"
#import "chrome/browser/cocoa/location_bar/location_bar_decoration.h"
#import "chrome/browser/cocoa/location_bar/location_icon_decoration.h"
#import "chrome/browser/cocoa/location_bar/selected_keyword_decoration.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::testing::StrictMock;
using ::testing::_;

namespace {

// Width of the field so that we don't have to ask |field_| for it all
// the time.
const CGFloat kWidth(300.0);

// A narrow width for tests which test things that don't fit.
const CGFloat kNarrowWidth(5.0);

class MockDecoration : public LocationBarDecoration {
 public:
  virtual CGFloat GetWidthForSpace(CGFloat width) { return 20.0; }
  virtual bool IsVisible() const { return visible_; }
  void SetVisible(bool visible) { visible_ = visible; }

  MOCK_METHOD2(DrawInFrame, void(NSRect frame, NSView* control_view));

  bool visible_;
};

// A testing subclass that doesn't bother hooking up the Page Action itself.
class TestPageActionImageView : public LocationBarViewMac::PageActionImageView {
 public:
  TestPageActionImageView() {}
  ~TestPageActionImageView() {}
};

class TestPageActionViewList : public LocationBarViewMac::PageActionViewList {
 public:
  TestPageActionViewList()
      : LocationBarViewMac::PageActionViewList(NULL, NULL, NULL) {}
  ~TestPageActionViewList() {
    // |~PageActionViewList()| calls delete on the contents of
    // |views_|, which here are refs to stack objects.
    views_.clear();
  }

  void Add(LocationBarViewMac::PageActionImageView* view) {
    views_.push_back(view);
  }
};

class AutocompleteTextFieldCellTest : public CocoaTest {
 public:
  AutocompleteTextFieldCellTest() : page_action_views_() {
    // Make sure this is wide enough to play games with the cell
    // decorations.
    const NSRect frame = NSMakeRect(0, 0, kWidth, 30);

    scoped_nsobject<NSTextField> view(
        [[NSTextField alloc] initWithFrame:frame]);
    view_ = view.get();

    scoped_nsobject<AutocompleteTextFieldCell> cell(
        [[AutocompleteTextFieldCell alloc] initTextCell:@"Testing"]);
    [cell setEditable:YES];
    [cell setBordered:YES];
    [cell setPageActionViewList:&page_action_views_];

    [cell clearDecorations];
    mock_left_decoration_.SetVisible(false);
    [cell addLeftDecoration:&mock_left_decoration_];

    [view_ setCell:cell.get()];

    [[test_window() contentView] addSubview:view_];
  }

  NSTextField* view_;
  MockDecoration mock_left_decoration_;
  TestPageActionViewList page_action_views_;
};

// Basic view tests (AddRemove, Display).
TEST_VIEW(AutocompleteTextFieldCellTest, view_);

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(AutocompleteTextFieldCellTest, FocusedDisplay) {
  [view_ display];

  // Test focused drawing.
  [test_window() makePretendKeyWindowAndSetFirstResponder:view_];
  [view_ display];
  [test_window() clearPretendKeyWindowAndFirstResponder];

  // Test display of various cell configurations.
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  [view_ display];

  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"prefix" image:image suffix:@"suffix"
              availableWidth:kWidth];
  [view_ display];

  // Load available decorations and try drawing.  To make sure that
  // they are actually drawn, check that |GetWidthForSpace()| doesn't
  // indicate that they should be omitted.
  const CGFloat kVeryWide = 1000.0;

  SelectedKeywordDecoration selected_keyword_decoration([view_ font]);
  selected_keyword_decoration.SetVisible(true);
  selected_keyword_decoration.SetKeyword(std::wstring(L"Google"), false);
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

  EVBubbleDecoration ev_bubble_decoration(&location_icon_decoration,
                                          [view_ font]);
  ev_bubble_decoration.SetVisible(true);
  ev_bubble_decoration.SetImage([NSImage imageNamed:@"NSApplicationIcon"]);
  ev_bubble_decoration.SetLabel(@"Application");
  [cell addLeftDecoration:&ev_bubble_decoration];
  EXPECT_NE(ev_bubble_decoration.GetWidthForSpace(kVeryWide),
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

TEST_F(AutocompleteTextFieldCellTest, ClearKeywordAndHint) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  EXPECT_FALSE([cell hintString]);

  // Check that the search hint can be cleared.
  [cell setSearchHintString:@"Type to search" availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  [cell clearHint];
  EXPECT_FALSE([cell hintString]);

  // Check that the keyword hint can be cleared.
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  [cell setKeywordHintPrefix:@"Press " image:image suffix:@" to search Engine"
              availableWidth:kWidth];
  EXPECT_TRUE([cell hintString]);
  [cell clearHint];
  EXPECT_FALSE([cell hintString]);
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

  // Text frame should take everything over again on reset.
  [cell clearHint];
  textFrame = [cell textFrameForFrame:bounds];
  EXPECT_FALSE(NSIsEmptyRect(textFrame));
  EXPECT_TRUE(NSContainsRect(bounds, textFrame));
  EXPECT_EQ(NSMinX(bounds), NSMinX(textFrame));
  EXPECT_EQ(NSMaxX(bounds), NSMaxX(textFrame));
  EXPECT_TRUE(NSContainsRect(cursorFrame, textFrame));

  // Search hint text takes precedence over the hint icon; the text frame
  // should be smaller in order to accomodate the text that is wider than
  // the icon.
  [cell setSearchHintString:@"Search hint" availableWidth:kWidth];
  NSRect textFrameWithHintText = [cell textFrameForFrame:bounds];
  EXPECT_TRUE(NSContainsRect(textFrame, textFrameWithHintText));
  EXPECT_LT(NSWidth(textFrameWithHintText), NSWidth(textFrame));

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

  [cell clearHint];
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));
  EXPECT_TRUE(NSEqualRects(drawingRect, originalDrawingRect));

  mock_left_decoration_.SetVisible(true);
  textFrame = [cell textFrameForFrame:bounds];
  drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_FALSE(NSIsEmptyRect(drawingRect));
  EXPECT_TRUE(NSContainsRect(NSInsetRect(textFrame, 1, 1), drawingRect));
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

// Test Page Action counts.
TEST_F(AutocompleteTextFieldCellTest, PageActionCount) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  TestPageActionImageView page_action_view;
  TestPageActionImageView page_action_view2;

  TestPageActionViewList list;
  [cell setPageActionViewList:&list];

  EXPECT_EQ(0u, [cell pageActionCount]);
  list.Add(&page_action_view);
  EXPECT_EQ(1u, [cell pageActionCount]);
  list.Add(&page_action_view2);
  EXPECT_EQ(2u, [cell pageActionCount]);
}

// Test that Page Action icons are properly placed.
TEST_F(AutocompleteTextFieldCellTest, PageActionImageFrame) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);
  const NSRect bounds([view_ bounds]);

  TestPageActionImageView page_action_view;
  // We'll assume that the extensions code enforces icons smaller than the
  // location bar.
  NSImage* image = [[[NSImage alloc]
      initWithSize:NSMakeSize(NSHeight(bounds) - 4, NSHeight(bounds) - 4)]
      autorelease];
  page_action_view.SetImage(image);

  TestPageActionImageView page_action_view2;
  page_action_view2.SetImage(image);

  TestPageActionViewList list;
  list.Add(&page_action_view);
  list.Add(&page_action_view2);
  [cell setPageActionViewList:&list];

  page_action_view.SetVisible(false);
  page_action_view2.SetVisible(false);
  EXPECT_TRUE(NSIsEmptyRect([cell pageActionFrameForIndex:0 inFrame:bounds]));
  EXPECT_TRUE(NSIsEmptyRect([cell pageActionFrameForIndex:1 inFrame:bounds]));

  // One page action, no lock icon.
  page_action_view.SetVisible(true);
  NSRect iconRect0 = [cell pageActionFrameForIndex:0 inFrame:bounds];

  EXPECT_FALSE(NSIsEmptyRect(iconRect0));
  EXPECT_TRUE(NSContainsRect(bounds, iconRect0));

  // Make sure we are right of the |drawingRect|.
  NSRect drawingRect = [cell drawingRectForBounds:bounds];
  EXPECT_LE(NSMaxX(drawingRect), NSMinX(iconRect0));

  // Make sure we're right of the |textFrame|.
  NSRect textFrame = [cell textFrameForFrame:bounds];
  EXPECT_LE(NSMaxX(textFrame), NSMinX(iconRect0));

  // Two page actions plus a security label.
  page_action_view2.SetVisible(true);
  NSArray* icons = [cell layedOutIcons:bounds];
  ASSERT_EQ(2u, [icons count]);

  // TODO(shess): page-action list is inverted from -layedOutIcons:
  // Yes, this is confusing, fix it.
  iconRect0 = [cell pageActionFrameForIndex:0 inFrame:bounds];
  NSRect iconRect1 = [cell pageActionFrameForIndex:1 inFrame:bounds];
  NSRect labelRect = [[icons objectAtIndex:0] rect];

  EXPECT_TRUE(NSEqualRects(iconRect0, [[icons objectAtIndex:1] rect]));
  EXPECT_TRUE(NSEqualRects(iconRect1, [[icons objectAtIndex:0] rect]));

  // Make sure they're all in the expected order, and right of the |drawingRect|
  // and |textFrame|.
  drawingRect = [cell drawingRectForBounds:bounds];
  textFrame = [cell textFrameForFrame:bounds];

  EXPECT_FALSE(NSIsEmptyRect(iconRect0));
  EXPECT_TRUE(NSContainsRect(bounds, iconRect0));
  EXPECT_FALSE(NSIsEmptyRect(iconRect1));
  EXPECT_TRUE(NSContainsRect(bounds, iconRect1));
  EXPECT_FALSE(NSIsEmptyRect(labelRect));
  EXPECT_TRUE(NSContainsRect(bounds, labelRect));

  EXPECT_LE(NSMaxX(drawingRect), NSMinX(iconRect1));
  EXPECT_LE(NSMaxX(textFrame), NSMinX(iconRect1));
  EXPECT_LE(NSMaxX(iconRect1), NSMinX(iconRect0));
  EXPECT_LE(NSMaxX(labelRect), NSMinX(iconRect0));
}

// Test that the cell drops the search hint if there is no room for
// it.
TEST_F(AutocompleteTextFieldCellTest, OmitsSearchHintIfNarrow) {
  AutocompleteTextFieldCell* cell =
      static_cast<AutocompleteTextFieldCell*>([view_ cell]);

  NSString* const kSearchHint = @"Type to search";

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

  NSString* const kHintPrefix = @"Press ";
  NSString* const kHintSuffix = @" to search Engine";

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
