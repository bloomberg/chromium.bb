// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/task_manager_mac.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/gfx/image/image_skia.h"

namespace {

class TestResource : public TaskManager::Resource {
 public:
  TestResource(const string16& title, pid_t pid) : title_(title), pid_(pid) {}
  virtual string16 GetTitle() const OVERRIDE { return title_; }
  virtual string16 GetProfileName() const OVERRIDE { return string16(); }
  virtual gfx::ImageSkia GetIcon() const OVERRIDE { return gfx::ImageSkia(); }
  virtual base::ProcessHandle GetProcess() const OVERRIDE { return pid_; }
  virtual int GetUniqueChildProcessId() const OVERRIDE {
    // In reality the unique child process ID is not the actual process ID,
    // but for testing purposes it shouldn't make difference.
    return static_cast<int>(base::GetCurrentProcId());
  }
  virtual Type GetType() const OVERRIDE { return RENDERER; }
  virtual bool SupportNetworkUsage() const OVERRIDE { return false; }
  virtual void SetSupportNetworkUsage() OVERRIDE { NOTREACHED(); }
  virtual void Refresh() OVERRIDE {}
  string16 title_;
  string16 profile_name_;
  pid_t pid_;
};

}  // namespace

class TaskManagerWindowControllerTest : public CocoaTest {
};

// Test creation, to ensure nothing leaks or crashes.
TEST_F(TaskManagerWindowControllerTest, Init) {
  TaskManager task_manager;
  TaskManagerMac* bridge(new TaskManagerMac(&task_manager, false));
  TaskManagerWindowController* controller = bridge->cocoa_controller();

  // Releases the controller, which in turn deletes |bridge|.
  [controller close];
}

TEST_F(TaskManagerWindowControllerTest, Sort) {
  TaskManager task_manager;

  TestResource resource1(UTF8ToUTF16("zzz"), 1);
  TestResource resource2(UTF8ToUTF16("zzb"), 2);
  TestResource resource3(UTF8ToUTF16("zza"), 2);

  task_manager.AddResource(&resource1);
  task_manager.AddResource(&resource2);
  task_manager.AddResource(&resource3);  // Will be in the same group as 2.

  TaskManagerMac* bridge(new TaskManagerMac(&task_manager, false));
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

  TestResource resource1(UTF8ToUTF16("yyy"), 1);
  TestResource resource2(UTF8ToUTF16("aaa"), 2);

  task_manager.AddResource(&resource1);
  task_manager.AddResource(&resource2);

  TaskManagerMac* bridge(new TaskManagerMac(&task_manager, false));
  TaskManagerWindowController* controller = bridge->cocoa_controller();
  NSTableView* table = [controller tableView];
  ASSERT_EQ(2, [controller numberOfRowsInTableView:table]);

  // Select row 0 in the table (corresponds to row 1 in the model).
  [table selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
     byExtendingSelection:NO];

  // Change the name of resource2 so that it becomes row 1 in the table.
  resource2.title_ = UTF8ToUTF16("zzz");
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
