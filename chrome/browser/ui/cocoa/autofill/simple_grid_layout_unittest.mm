// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/simple_grid_layout.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Layout objects as multiple columns in a single row.
class ColumnLayout : public SimpleGridLayout {
 public:
   ColumnLayout(NSView* host)  : SimpleGridLayout(host) { AddRow(); }
   ~ColumnLayout() {}
};

class LayoutTest : public testing::Test {
 public:
  base::scoped_nsobject<NSView> CreateViewWithWidth(float width) {
    // Height of 1, since empty rects become 0x0 when run by NSIntegralRect.
    NSRect frame = NSMakeRect(0, 0, width, 1);
    base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:frame]);
    return view;
  }

  // Width of 1, since empty rects become 0x0 when run by NSIntegralRect.
  base::scoped_nsobject<NSView> CreateViewWithHeight(float height) {
    NSRect frame = NSMakeRect(0, 0, 1, height);
    base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:frame]);
    return view;
  }
};

TEST_F(LayoutTest, GridLayoutAddGetColumnSet) {
  SimpleGridLayout layout(NULL);

  // Non-existing id's are not found.
  EXPECT_EQ(0L, layout.GetColumnSet(1));

  // Created ColumnSets can be retrieved.
  ColumnSet* set1 = layout.AddColumnSet(10);
  ColumnSet* set2 = layout.AddColumnSet(11);
  ColumnSet* set3 = layout.AddColumnSet(12);
  EXPECT_EQ(set2, layout.GetColumnSet(11));
  EXPECT_EQ(set1, layout.GetColumnSet(10));
  EXPECT_EQ(set3, layout.GetColumnSet(12));
}

TEST_F(LayoutTest, AddRowAutogeneratesColumnSetId) {
  SimpleGridLayout layout(NULL);

  ColumnSet* set1 = layout.AddRow();
  EXPECT_NE(0, set1->id());

  ColumnSet* set2 = layout.AddRow();
  EXPECT_NE(set1->id(), set2->id());
}

TEST_F(LayoutTest, AddRowResetsNextColumn) {
  SimpleGridLayout layout(NULL);
  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.AddView(nil);
  EXPECT_EQ(1, layout.next_column());

  layout.AddRow();
  EXPECT_EQ(0, layout.next_column());
}

TEST_F(LayoutTest, GetLastValidColumnSet) {
  SimpleGridLayout layout(NULL);

  EXPECT_EQ(NULL, layout.GetLastValidColumnSet());

  ColumnSet* cs1 = layout.AddColumnSet(1);
  ColumnSet* cs2 = layout.AddColumnSet(2);
  layout.StartRow(0.0, 2);
  EXPECT_EQ(cs2, layout.GetLastValidColumnSet());

  layout.AddPaddingRow(10);
  EXPECT_EQ(cs2, layout.GetLastValidColumnSet());

  layout.StartRow(0.0, 1);
  EXPECT_EQ(cs1, layout.GetLastValidColumnSet());
}


TEST_F(LayoutTest, AddRowCreatesNewColumnSet) {
  SimpleGridLayout layout(NULL);

  EXPECT_EQ(NULL, layout.GetLastValidColumnSet());

  ColumnSet* old_column_set = layout.AddRow();
  EXPECT_TRUE(old_column_set != NULL);
  EXPECT_EQ(old_column_set, layout.GetLastValidColumnSet());

  ColumnSet* new_column_set = layout.AddRow();
  EXPECT_TRUE(new_column_set != NULL);
  EXPECT_EQ(new_column_set, layout.GetLastValidColumnSet());
}

TEST_F(LayoutTest, AddPaddingRow) {
  SimpleGridLayout layout(NULL);

  ColumnSet* cs;
  cs = layout.AddRow();
  cs->AddColumn(1.0f);
  base::scoped_nsobject<NSView> view1 = CreateViewWithHeight(30.0f);
  layout.AddView(view1);

  layout.AddPaddingRow(20);

  cs = layout.AddRow();
  cs->AddColumn(1.0f);
  EXPECT_EQ(3, layout.num_rows());
  base::scoped_nsobject<NSView> view2 = CreateViewWithHeight(10.0f);
  layout.AddView(view2);

  layout.SizeRowsAndColumns(0.0f);
  EXPECT_FLOAT_EQ(30.0f, layout.GetRowHeight(0));
  EXPECT_FLOAT_EQ(20.0f, layout.GetRowHeight(1));
  EXPECT_FLOAT_EQ(10.0f, layout.GetRowHeight(2));
}

TEST_F(LayoutTest, RowsAccomodateHeightOfAllElements) {
  SimpleGridLayout layout(NULL);

  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);

  base::scoped_nsobject<NSView> view1 = CreateViewWithHeight(37.0f);
  layout.AddView(view1);
  layout.SizeRowsAndColumns(0.0f);
  EXPECT_FLOAT_EQ(37.0f, layout.GetRowHeight(0));

  base::scoped_nsobject<NSView> view2 = CreateViewWithHeight(26.0f);
  layout.AddView(view2);
  layout.SizeRowsAndColumns(0.0f);
  EXPECT_FLOAT_EQ(37.0f, layout.GetRowHeight(0));

  base::scoped_nsobject<NSView> view3 = CreateViewWithHeight(42.0f);
  layout.AddView(view3);
  layout.SizeRowsAndColumns(0.0f);
  EXPECT_FLOAT_EQ(42.0f, layout.GetRowHeight(0));
}

TEST_F(LayoutTest, SizeRowsAdjustsRowLocations) {
  SimpleGridLayout layout(NULL);

  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  base::scoped_nsobject<NSView> view1 = CreateViewWithHeight(30.0f);
  layout.AddView(view1);

  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  base::scoped_nsobject<NSView> view2 = CreateViewWithHeight(40.0f);
  layout.AddView(view2);

  layout.SizeRowsAndColumns(0.0f);
  EXPECT_FLOAT_EQ(0.0f, layout.GetRowLocation(0));
  EXPECT_FLOAT_EQ(30.0f, layout.GetRowLocation(1));
}

TEST_F(LayoutTest, SimpleGridLayoutAdjustsViews) {
  SimpleGridLayout layout(NULL);

  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);

  base::scoped_nsobject<NSView> view1 = CreateViewWithHeight(22.0f);
  base::scoped_nsobject<NSView> view2 = CreateViewWithHeight(20.0f);
  layout.AddView(view1);
  layout.AddView(view2);

  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  base::scoped_nsobject<NSView> view3 = CreateViewWithHeight(18.0f);
  layout.AddView(view3);

  base::scoped_nsobject<NSView> host_view = CreateViewWithWidth(150.0f);
  layout.Layout(host_view);

  EXPECT_FLOAT_EQ(72.0f, NSWidth([view1 frame]));
  EXPECT_FLOAT_EQ(22.0f, NSHeight([view1 frame]));
  EXPECT_FLOAT_EQ(0.0f, NSMinX([view1 frame]));
  EXPECT_FLOAT_EQ(0.0f, NSMinY([view1 frame]));

  EXPECT_FLOAT_EQ(48.0f, NSWidth([view2 frame]));
  EXPECT_FLOAT_EQ(22.0f, NSHeight([view2 frame]));
  EXPECT_FLOAT_EQ(102.0f, NSMinX([view2 frame]));
  EXPECT_FLOAT_EQ(0.0f, NSMinY([view2 frame]));

  EXPECT_FLOAT_EQ(150.0f, NSWidth([view3 frame]));
  EXPECT_FLOAT_EQ(18.0f, NSHeight([view3 frame]));
  EXPECT_FLOAT_EQ(0.0f, NSMinX([view3 frame]));
  EXPECT_FLOAT_EQ(22.0f, NSMinY([view3 frame]));
}

TEST_F(LayoutTest, PreferredHeightForEmptyLayout) {
  SimpleGridLayout layout(NULL);
  EXPECT_FLOAT_EQ(0.0f, layout.GetPreferredHeightForWidth(100.0));
}

TEST_F(LayoutTest, SimpleGridLayoutHeightForWidth)
{
  SimpleGridLayout layout(NULL);
  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);

  base::scoped_nsobject<NSView> view1 = CreateViewWithHeight(22.0f);
  layout.AddView(view1);
  EXPECT_FLOAT_EQ(22.0f, layout.GetPreferredHeightForWidth(100.0));

  layout.AddPaddingRow(12);
  EXPECT_FLOAT_EQ(34.0f, layout.GetPreferredHeightForWidth(100.0));

  layout.AddRow();
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  base::scoped_nsobject<NSView> view2 = CreateViewWithHeight(13.0f);
  layout.AddView(view2);
  EXPECT_FLOAT_EQ(47.0f, layout.GetPreferredHeightForWidth(100.0));
}

TEST_F(LayoutTest, SkipColumns) {
  ColumnLayout layout(NULL);

  ASSERT_TRUE(layout.GetLastValidColumnSet() != NULL);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);

  // Skip a single column
  EXPECT_EQ(0, layout.next_column());
  layout.SkipColumns(1);
  EXPECT_EQ(1, layout.next_column());

  // Skip multiple columns
  layout.SkipColumns(2);
  EXPECT_EQ(3, layout.next_column());

  // SkipColumns skips padding, too
  layout.SkipColumns(1);
  EXPECT_EQ(5, layout.next_column());
}

TEST_F(LayoutTest, SkipPaddingColumns) {
  ColumnLayout layout(NULL);

  ASSERT_TRUE(layout.GetLastValidColumnSet() != NULL);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);

  // Don't skip over non-padding columns.
  EXPECT_EQ(0, layout.next_column());
  layout.SkipPaddingColumns();
  EXPECT_EQ(0, layout.next_column());

  // Skip a single padding column.
  layout.AdvanceColumn();
  EXPECT_EQ(1, layout.next_column());
  layout.SkipPaddingColumns();
  EXPECT_EQ(2, layout.next_column());

  // Skip multiple padding columns.
  layout.AdvanceColumn();
  EXPECT_EQ(3, layout.next_column());
  layout.SkipPaddingColumns();
  EXPECT_EQ(5, layout.next_column());
}

TEST_F(LayoutTest, SizeColumnsAdjustLocationAndSize) {
  ColumnLayout layout(NULL);

  ASSERT_TRUE(layout.GetLastValidColumnSet() != NULL);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);

  layout.SizeRowsAndColumns(130.0f);
  EXPECT_FLOAT_EQ(0.0f, layout.GetLastValidColumnSet()->ColumnLocation(0));
  EXPECT_FLOAT_EQ(60.0f, layout.GetLastValidColumnSet()->GetColumnWidth(0));
  EXPECT_FLOAT_EQ(60.0f, layout.GetLastValidColumnSet()->ColumnLocation(1));
  EXPECT_FLOAT_EQ(30.0f, layout.GetLastValidColumnSet()->GetColumnWidth(1));
  EXPECT_FLOAT_EQ(90.0f, layout.GetLastValidColumnSet()->ColumnLocation(2));
  EXPECT_FLOAT_EQ(40.0f, layout.GetLastValidColumnSet()->GetColumnWidth(2));
}

TEST_F(LayoutTest, AddViewSkipsPaddingColumns) {
  ColumnLayout layout(NULL);

  ASSERT_TRUE(layout.GetLastValidColumnSet() != NULL);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);

  EXPECT_EQ(0, layout.next_column());
  base::scoped_nsobject<NSView> view1 = CreateViewWithWidth(37.0f);
  layout.AddView(view1);
  EXPECT_EQ(2, layout.next_column());
}

TEST_F(LayoutTest, ColumnLayoutAdjustsViews) {
  ColumnLayout layout(NULL);

  ASSERT_TRUE(layout.GetLastValidColumnSet() != NULL);
  layout.GetLastValidColumnSet()->AddColumn(0.6f);
  layout.GetLastValidColumnSet()->AddPaddingColumn(30);
  layout.GetLastValidColumnSet()->AddColumn(0.4f);

  base::scoped_nsobject<NSView> view1 = CreateViewWithWidth(37.0f);
  base::scoped_nsobject<NSView> view2 = CreateViewWithWidth(42.0f);
  layout.AddView(view1);
  layout.AddView(view2);

  base::scoped_nsobject<NSView> host_view = CreateViewWithWidth(150.0f);
  layout.Layout(host_view);

  EXPECT_FLOAT_EQ(72.0f, NSWidth([view1 frame]));
  EXPECT_FLOAT_EQ(48.0f, NSWidth([view2 frame]));
}

TEST_F(LayoutTest, ColumnSetAddPadding) {
  ColumnSet columns(0);

  columns.AddPaddingColumn(20);
  EXPECT_EQ(1, columns.num_columns());

  columns.CalculateSize(20.0f);
  EXPECT_FLOAT_EQ(20.0f, columns.GetColumnWidth(0));

  columns.CalculateSize(120.0f);
  EXPECT_FLOAT_EQ(20.0f, columns.GetColumnWidth(0));
}

TEST_F(LayoutTest, ColumnSetOneFlexibleColumn) {
  ColumnSet columns(0);

  columns.AddColumn(1.0f);
  EXPECT_EQ(1, columns.num_columns());

  columns.CalculateSize(20.0f);
  EXPECT_FLOAT_EQ(20.0f, columns.GetColumnWidth(0));

  columns.CalculateSize(120.0f);
  EXPECT_FLOAT_EQ(120.0f, columns.GetColumnWidth(0));
}

TEST_F(LayoutTest, ColumnSetTwoFlexibleColumns) {
  ColumnSet columns(0);

  columns.AddColumn(0.6f);
  columns.AddColumn(0.4f);
  EXPECT_EQ(2, columns.num_columns());

  columns.CalculateSize(100.0f);
  EXPECT_FLOAT_EQ(60.0f, columns.GetColumnWidth(0));
  EXPECT_FLOAT_EQ(40.0f, columns.GetColumnWidth(1));

  columns.CalculateSize(150.0f);
  EXPECT_FLOAT_EQ(90.0f, columns.GetColumnWidth(0));
  EXPECT_FLOAT_EQ(60.0f, columns.GetColumnWidth(1));
}

TEST_F(LayoutTest, ColumnSetMixedColumns) {
  ColumnSet columns(0);

  columns.AddColumn(0.6f);
  columns.AddPaddingColumn(20);
  columns.AddColumn(0.4f);
  EXPECT_EQ(3, columns.num_columns());

  columns.CalculateSize(100.0f);
  EXPECT_FLOAT_EQ(48.0f, columns.GetColumnWidth(0));
  EXPECT_FLOAT_EQ(20.0f, columns.GetColumnWidth(1));
  EXPECT_FLOAT_EQ(32.0f, columns.GetColumnWidth(2));

  columns.CalculateSize(200.0f);
  EXPECT_FLOAT_EQ(108.0f, columns.GetColumnWidth(0));
  EXPECT_FLOAT_EQ(20.0f, columns.GetColumnWidth(1));
  EXPECT_FLOAT_EQ(72.0f, columns.GetColumnWidth(2));
}

}  // namespace
