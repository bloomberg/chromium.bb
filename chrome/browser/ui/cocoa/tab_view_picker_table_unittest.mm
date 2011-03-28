// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/tab_view_picker_table.h"

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

@interface TabViewPickerTableTestPing : NSObject <NSTabViewDelegate> {
 @public
  BOOL didSelectItemCalled_;
}
@end

@implementation TabViewPickerTableTestPing
- (void)         tabView:(NSTabView*)tabView
    didSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  didSelectItemCalled_ = YES;
}
@end

namespace {

class TabViewPickerTableTest : public CocoaTest {
 public:
  TabViewPickerTableTest() {
    // Initialize picker table.
    NSRect frame = NSMakeRect(0, 0, 30, 50);
    scoped_nsobject<TabViewPickerTable> view(
        [[TabViewPickerTable alloc] initWithFrame:frame]);
    view_ = view.get();
    [view_ setAllowsEmptySelection:NO];
    [view_ setAllowsMultipleSelection:NO];
    [view_ addTableColumn:
        [[[NSTableColumn alloc] initWithIdentifier:nil] autorelease]];
    [[test_window() contentView] addSubview:view_];

    // Initialize source tab view, with delegate.
    frame = NSMakeRect(30, 0, 50, 50);
    scoped_nsobject<NSTabView> tabView(
        [[NSTabView alloc] initWithFrame:frame]);
    tabView_ = tabView.get();

    scoped_nsobject<NSTabViewItem> item1(
        [[NSTabViewItem alloc] initWithIdentifier:nil]);
    [item1 setLabel:@"label 1"];
    [tabView_ addTabViewItem:item1];

    scoped_nsobject<NSTabViewItem> item2(
        [[NSTabViewItem alloc] initWithIdentifier:nil]);
    [item2 setLabel:@"label 2"];
    [tabView_ addTabViewItem:item2];

    [tabView_ selectTabViewItemAtIndex:1];
    [[test_window() contentView] addSubview:tabView_];

    ping_.reset([TabViewPickerTableTestPing new]);
    [tabView_ setDelegate:ping_.get()];

    // Simulate nib loading.
    view_->tabView_ = tabView_;
    [view_ awakeFromNib];
  }

  TabViewPickerTable* view_;
  NSTabView* tabView_;
  scoped_nsobject<TabViewPickerTableTestPing> ping_;
};

TEST_VIEW(TabViewPickerTableTest, view_)

TEST_F(TabViewPickerTableTest, TestInitialSelectionCorrect) {
  EXPECT_EQ(1, [view_ selectedRow]);
}

TEST_F(TabViewPickerTableTest, TestSelectionUpdates) {
  [tabView_ selectTabViewItemAtIndex:0];
  EXPECT_EQ(0, [view_ selectedRow]);

  [tabView_ selectTabViewItemAtIndex:1];
  EXPECT_EQ(1, [view_ selectedRow]);
}

TEST_F(TabViewPickerTableTest, TestDelegateStillWorks) {
  EXPECT_FALSE(ping_.get()->didSelectItemCalled_);
  [tabView_ selectTabViewItemAtIndex:0];
  EXPECT_TRUE(ping_.get()->didSelectItemCalled_);
}

TEST_F(TabViewPickerTableTest, RowsCorrect) {
  EXPECT_EQ(2, [view_ numberOfRows]);
  EXPECT_EQ(2,
            [[view_ dataSource] outlineView:view_ numberOfChildrenOfItem:nil]);

  id item;
  item = [[view_ dataSource] outlineView:view_ child:0 ofItem:nil];
  EXPECT_NSEQ(@"label 1",
              [[view_ dataSource] outlineView:view_
                    objectValueForTableColumn:nil  // ignored
                                       byItem:item]);
  item = [[view_ dataSource] outlineView:view_ child:1 ofItem:nil];
  EXPECT_NSEQ(@"label 2",
              [[view_ dataSource] outlineView:view_
                    objectValueForTableColumn:nil  // ignored
                                       byItem:item]);
}

TEST_F(TabViewPickerTableTest, TestListUpdatesTabView) {
  [view_   selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
      byExtendingSelection:NO];
  EXPECT_EQ(0, [view_ selectedRow]);  // sanity
  EXPECT_EQ(0, [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]]);
}

TEST_F(TabViewPickerTableTest, TestWithHeadingNotEmpty) {
  [view_ setHeading:@"disregard this"];

  EXPECT_EQ(2, [view_ selectedRow]);

  [tabView_ selectTabViewItemAtIndex:0];
  EXPECT_EQ(1, [view_ selectedRow]);
  [tabView_ selectTabViewItemAtIndex:1];
  EXPECT_EQ(2, [view_ selectedRow]);

  [view_   selectRowIndexes:[NSIndexSet indexSetWithIndex:1]
      byExtendingSelection:NO];
  EXPECT_EQ(1, [view_ selectedRow]);  // sanity
  EXPECT_EQ(0, [tabView_ indexOfTabViewItem:[tabView_ selectedTabViewItem]]);
}

}  // namespace
