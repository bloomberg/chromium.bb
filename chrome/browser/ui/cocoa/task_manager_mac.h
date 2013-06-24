// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/cocoa/table_row_nsimage_cache.h"

@class WindowSizeAutosaver;
class TaskManagerMac;

namespace gfx {
class ImageSkia;
}

// This class is responsible for loading the task manager window and for
// managing it.
@interface TaskManagerWindowController :
  NSWindowController<NSTableViewDataSource,
                     NSTableViewDelegate> {
 @private
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* endProcessButton_;
  TaskManagerMac* taskManagerObserver_;  // weak
  TaskManager* taskManager_;  // weak
  TaskManagerModel* model_;  // weak

  base::scoped_nsobject<WindowSizeAutosaver> size_saver_;

  // These contain a permutation of [0..|model_->ResourceCount() - 1|]. Used to
  // implement sorting.
  std::vector<int> viewToModelMap_;
  std::vector<int> modelToViewMap_;

  // Descriptor of the current sort column.
  base::scoped_nsobject<NSSortDescriptor> currentSortDescriptor_;
}

// Creates and shows the task manager's window.
- (id)initWithTaskManagerObserver:(TaskManagerMac*)taskManagerObserver;

// Refreshes all data in the task manager table.
- (void)reloadData;

// Callback for "Stats for nerds" link.
- (IBAction)statsLinkClicked:(id)sender;

// Callback for "End process" button.
- (IBAction)killSelectedProcesses:(id)sender;

// Callback for double clicks on the table.
- (void)selectDoubleClickedTab:(id)sender;
@end

@interface TaskManagerWindowController (TestingAPI)
- (NSTableView*)tableView;
@end

// This class listens to task changed events sent by chrome.
class TaskManagerMac : public TaskManagerModelObserver,
                       public TableRowNSImageCache::Table {
 public:
  explicit TaskManagerMac(TaskManager* task_manager);
  virtual ~TaskManagerMac();

  // TaskManagerModelObserver
  virtual void OnModelChanged() OVERRIDE;
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  virtual void OnItemsRemoved(int start, int length) OVERRIDE;

  // Called by the cocoa window controller when its window closes and the
  // controller destroyed itself. Informs the model to stop updating.
  void WindowWasClosed();

  // TableRowNSImageCache::Table
  virtual int RowCount() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon(int r) const OVERRIDE;

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static void Show();

  // Returns the TaskManager observed by |this|.
  TaskManager* task_manager() { return task_manager_; }

  // Lazily converts the image at the given row and caches it in |icon_cache_|.
  NSImage* GetImageForRow(int row);

  // Returns the cocoa object. Used for testing.
  TaskManagerWindowController* cocoa_controller() { return window_controller_; }

 private:
  // The task manager.
  TaskManager* const task_manager_;  // weak

  // Our model.
  TaskManagerModel* const model_;  // weak

  // Controller of our window, destroys itself when the task manager window
  // is closed.
  TaskManagerWindowController* window_controller_;  // weak

  // Caches favicons for all rows. Needs to be initalized after |model_|.
  TableRowNSImageCache icon_cache_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerMac* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_TASK_MANAGER_MAC_H_
