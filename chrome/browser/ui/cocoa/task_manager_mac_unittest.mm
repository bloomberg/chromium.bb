// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/resource_provider.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/task_manager_mac.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/gfx/image/image_skia.h"

@interface TaskManagerWindowController(UnitTest)

- (void)toggleColumn:(NSMenuItem*)sender;

@end

namespace {

class TestResource : public task_manager::Resource {
 public:
  TestResource(const base::string16& title, pid_t pid)
      : title_(title), pid_(pid) {}
  base::string16 GetTitle() const override { return title_; }
  base::string16 GetProfileName() const override { return base::string16(); }
  gfx::ImageSkia GetIcon() const override { return gfx::ImageSkia(); }
  base::ProcessHandle GetProcess() const override { return pid_; }
  int GetUniqueChildProcessId() const override {
    // In reality the unique child process ID is not the actual process ID,
    // but for testing purposes it shouldn't make difference.
    return static_cast<int>(base::GetCurrentProcId());
  }
  Type GetType() const override { return RENDERER; }
  bool SupportNetworkUsage() const override { return false; }
  void SetSupportNetworkUsage() override { NOTREACHED(); }
  void Refresh() override {}
  base::string16 title_;
  base::string16 profile_name_;
  pid_t pid_;
};

}  // namespace

class TaskManagerWindowControllerTest : public CocoaTest {
  content::TestBrowserThreadBundle thread_bundle_;
};

// Test creation, to ensure nothing leaks or crashes.
TEST_F(TaskManagerWindowControllerTest, Init) {
  TaskManager task_manager;
  TaskManagerMac* bridge(new TaskManagerMac(&task_manager));
  TaskManagerWindowController* controller = bridge->cocoa_controller();

  // Releases the controller, which in turn deletes |bridge|.
  [controller close];
}

TEST_F(TaskManagerWindowControllerTest, Sort) {
  TaskManager task_manager;

  TestResource resource1(base::UTF8ToUTF16("zzz"), 1);
  TestResource resource2(base::UTF8ToUTF16("zzb"), 2);
  TestResource resource3(base::UTF8ToUTF16("zza"), 2);

  task_manager.AddResource(&resource1);
  task_manager.AddResource(&resource2);
  task_manager.AddResource(&resource3);  // Will be in the same group as 2.

  TaskManagerMac* bridge(new TaskManagerMac(&task_manager));
  TaskManagerWindowController* controller = bridge->cocoa_controller();
  NSTableView* table = [controller tableView];
  ASSERT_EQ(3, [controller numberOfRowsInTableView:table]);

  // Test that table is sorted on title.
  NSTableColumn* title_column = [table tableColumnWithIdentifier:
      [NSString stringWithFormat:@"%d", IDS_TASK_MANAGER_TASK_COLUMN]];
  NSCell* cell;
  cell = [controller tableView:table dataCellForTableColumn:title_column row:0];
  EXPECT_NSEQ(@"zzb", [cell title]);
  cell = [controller tableView:table dataCellForTableColumn:title_column row:1];
  EXPECT_NSEQ(@"zza", [cell title]);
  cell = [controller tableView:table dataCellForTableColumn:title_column row:2];
  EXPECT_NSEQ(@"zzz", [cell title]);

  // Releases the controller, which in turn deletes |bridge|.
  [controller close];

  task_manager.RemoveResource(&resource1);
  task_manager.RemoveResource(&resource2);
  task_manager.RemoveResource(&resource3);
}

TEST_F(TaskManagerWindowControllerTest, SelectionAdaptsToSorting) {
  TaskManager task_manager;

  TestResource resource1(base::UTF8ToUTF16("yyy"), 1);
  TestResource resource2(base::UTF8ToUTF16("aaa"), 2);

  task_manager.AddResource(&resource1);
  task_manager.AddResource(&resource2);

  TaskManagerMac* bridge(new TaskManagerMac(&task_manager));
  TaskManagerWindowController* controller = bridge->cocoa_controller();
  NSTableView* table = [controller tableView];
  ASSERT_EQ(2, [controller numberOfRowsInTableView:table]);

  // Select row 0 in the table (corresponds to row 1 in the model).
  [table selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
     byExtendingSelection:NO];

  // Change the name of resource2 so that it becomes row 1 in the table.
  resource2.title_ = base::UTF8ToUTF16("zzz");
  bridge->task_manager()->model()->Refresh();
  bridge->OnItemsChanged(1, 1);

  // Check that the selection has moved to row 1.
  NSIndexSet* selection = [table selectedRowIndexes];
  ASSERT_EQ(1u, [selection count]);
  EXPECT_EQ(1u, [selection firstIndex]);

  // Releases the controller, which in turn deletes |bridge|.
  [controller close];

  task_manager.RemoveResource(&resource1);
  task_manager.RemoveResource(&resource2);
}

TEST_F(TaskManagerWindowControllerTest, EnsureNewPrimarySortColumn) {
  TaskManager task_manager;

  // Add a couple rows of data.
  TestResource resource1(base::UTF8ToUTF16("yyy"), 1);
  TestResource resource2(base::UTF8ToUTF16("aaa"), 2);

  task_manager.AddResource(&resource1);
  task_manager.AddResource(&resource2);

  TaskManagerMac* bridge(new TaskManagerMac(&task_manager));
  TaskManagerWindowController* controller = bridge->cocoa_controller();
  NSTableView* table = [controller tableView];
  ASSERT_EQ(2, [controller numberOfRowsInTableView:table]);

  // Locate the current first visible column.
  NSTableColumn* firstVisibleColumn = nil;
  for (NSTableColumn* nextColumn in [table tableColumns]) {
    if (![nextColumn isHidden]) {
      firstVisibleColumn = nextColumn;
      break;
    }
  }
  ASSERT_TRUE(firstVisibleColumn != nil);

  // Make the first visible column the primary sort column.
  NSSortDescriptor* sortDescriptor =
      [firstVisibleColumn sortDescriptorPrototype];
  ASSERT_TRUE(sortDescriptor != nil);
  [table setSortDescriptors:[NSArray arrayWithObject:sortDescriptor]];

  // Toggle the first visible column so that it's no longer visible, and make
  // sure a different column is now the primary sort column.
  NSMenuItem* menuItem = [[[NSMenuItem alloc]
                             initWithTitle:@"Temp"
                                    action:@selector(toggleColumn:)
                             keyEquivalent:@""] autorelease];
  [menuItem setRepresentedObject:firstVisibleColumn];
  [menuItem setState:NSOnState];
  [controller toggleColumn:menuItem];

  NSTableColumn* newFirstVisibleColumn = nil;
  for (NSTableColumn* nextColumn in [table tableColumns]) {
    if (![nextColumn isHidden]) {
      newFirstVisibleColumn = nextColumn;
      break;
    }
  }
  ASSERT_TRUE(newFirstVisibleColumn != nil);
  ASSERT_TRUE(newFirstVisibleColumn != firstVisibleColumn);
  NSSortDescriptor* newFirstSortDescriptor =
      [[table sortDescriptors] objectAtIndex:0];
  EXPECT_TRUE([newFirstSortDescriptor isEqual:
      [newFirstVisibleColumn sortDescriptorPrototype]]);

  // Release the controller, which in turn deletes |bridge|.
  [controller close];

  task_manager.RemoveResource(&resource1);
  task_manager.RemoveResource(&resource2);
}

TEST_F(TaskManagerWindowControllerTest, EnsureOneColumnVisible) {
  TaskManager task_manager;

  // Add a couple rows of data.
  TestResource resource1(base::UTF8ToUTF16("yyy"), 1);
  TestResource resource2(base::UTF8ToUTF16("aaa"), 2);

  task_manager.AddResource(&resource1);
  task_manager.AddResource(&resource2);

  TaskManagerMac* bridge(new TaskManagerMac(&task_manager));
  TaskManagerWindowController* controller = bridge->cocoa_controller();
  NSTableView* table = [controller tableView];
  ASSERT_EQ(2, [controller numberOfRowsInTableView:table]);

  // Toggle each visible column so that it's not visible.
  NSMenuItem* menuItem = [[[NSMenuItem alloc]
                             initWithTitle:@"Temp"
                                    action:@selector(toggleColumn:)
                             keyEquivalent:@""] autorelease];
  for (NSTableColumn* nextColumn in [table tableColumns]) {
    if (![nextColumn isHidden]) {
      [menuItem setState:NSOnState];
      [menuItem setRepresentedObject:nextColumn];
      [controller toggleColumn:menuItem];
    }
  }

  // Locate the one column that should still be visible.
  NSTableColumn* firstVisibleColumn = nil;
  for (NSTableColumn* nextColumn in [table tableColumns]) {
    if (![nextColumn isHidden]) {
      firstVisibleColumn = nextColumn;
      break;
    }
  }
  EXPECT_TRUE(firstVisibleColumn != nil);

  // Release the controller, which in turn deletes |bridge|.
  [controller close];

  task_manager.RemoveResource(&resource1);
  task_manager.RemoveResource(&resource2);
}
