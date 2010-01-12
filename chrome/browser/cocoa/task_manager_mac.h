// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TASK_MANAGER_MAC_H_
#define CHROME_BROWSER_COCOA_TASK_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"
#include "chrome/browser/task_manager.h"

class TaskManagerMac;

// This class is responsible for loading the task manager window and for
// managing it.
@interface TaskManagerWindowController : NSWindowController {
 @private
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* endProcessButton_;
  TaskManagerMac* taskManagerObserver_;  // weak
  TaskManager* taskManager_;  // weak
  TaskManagerModel* model_;  // weak
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

// This class listens to task changed events sent by chrome.
class TaskManagerMac : public TaskManagerModelObserver {
 public:
  TaskManagerMac();
  virtual ~TaskManagerMac();

  // TaskManagerModelObserver
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // Called by the cocoa window controller when its window closes and the
  // controller destroyed itself. Informs the model to stop updating.
  void WindowWasClosed();

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static void Show();

  // Returns the TaskManager observed by |this|.
  TaskManager* task_manager() { return task_manager_; }

 private:
  // The task manager.
   TaskManager* const task_manager_;  // weak

  // Our model.
  TaskManagerModel* const model_;  // weak

  // Controller of our window, destroys itself when the task manager window
  // is closed.
  TaskManagerWindowController* window_controller_;  // weak

  // An open task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerMac* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerMac);
};

#endif  // CHROME_BROWSER_COCOA_TASK_MANAGER_MAC_H_
