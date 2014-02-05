// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/omnibox/omnibox_popup_matrix.h"

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace {

NSEvent* MouseEventInRow(NSMatrix* matrix, NSEventType type, NSInteger row) {
  NSRect cell_rect = [matrix cellFrameAtRow:row column:0];
  NSPoint point_in_view = NSMakePoint(NSMidX(cell_rect), NSMidY(cell_rect));
  NSPoint point_in_window = [matrix convertPoint:point_in_view toView:nil];
  return cocoa_test_event_utils::MouseEventAtPoint(
      point_in_window, type, 0);
}

class OmniboxPopupMatrixTest : public CocoaTest,
                               public OmniboxPopupMatrixDelegate {
 public:
  OmniboxPopupMatrixTest()
      : selected_row_(0),
        clicked_row_(0),
        middle_clicked_row_(0) {
  }

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();
    matrix_.reset([[OmniboxPopupMatrix alloc] initWithDelegate:this]);
    [[test_window() contentView] addSubview:matrix_];
  };

  virtual void OnMatrixRowSelected(OmniboxPopupMatrix* matrix,
                                   size_t row) OVERRIDE {
    selected_row_ = row;
    [matrix_ selectCellAtRow:row column:0];
  }

  virtual void OnMatrixRowClicked(OmniboxPopupMatrix* matrix,
                                size_t row) OVERRIDE {
    clicked_row_ = row;
  }

  virtual void OnMatrixRowMiddleClicked(OmniboxPopupMatrix* matrix,
                                        size_t row) OVERRIDE {
    middle_clicked_row_ = row;
  }

 protected:
  base::scoped_nsobject<OmniboxPopupMatrix> matrix_;
  size_t selected_row_;
  size_t clicked_row_;
  size_t middle_clicked_row_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupMatrixTest);
};

TEST_VIEW(OmniboxPopupMatrixTest, matrix_);

TEST_F(OmniboxPopupMatrixTest, HighlightedRow) {
  [matrix_ renewRows:3 columns:1];
  EXPECT_EQ(-1, [matrix_ highlightedRow]);

  [matrix_ mouseEntered:MouseEventInRow(matrix_, NSMouseMoved, 0)];
  EXPECT_EQ(0, [matrix_ highlightedRow]);
  [matrix_ mouseEntered:MouseEventInRow(matrix_, NSMouseMoved, 2)];
  EXPECT_EQ(2, [matrix_ highlightedRow]);

  [matrix_ mouseExited:MouseEventInRow(matrix_, NSMouseMoved, 2)];
  EXPECT_EQ(-1, [matrix_ highlightedRow]);
}

TEST_F(OmniboxPopupMatrixTest, SelectedRow) {
  [matrix_ renewRows:3 columns:1];

  [NSApp postEvent:MouseEventInRow(matrix_, NSLeftMouseUp, 2) atStart:YES];
  [matrix_ mouseDown:MouseEventInRow(matrix_, NSLeftMouseDown, 2)];

  EXPECT_EQ(2u, selected_row_);
  EXPECT_EQ(2u, clicked_row_);
  EXPECT_EQ(0u, middle_clicked_row_);
}

}  // namespace
