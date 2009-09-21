// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/task_manager_mac.h"

#include <algorithm>
#include <vector>

#include "app/l10n_util_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "grit/generated_resources.h"

// TODO(thakis): Autoremember window size/pos (and selected columns?)
// TODO(thakis): Link that opens about:memory
// TODO(thakis): Activate button iff something is selected, hook it up
// TODO(thakis): Column sort comparator
// TODO(thakis): Clicking column header doesn't sort
// TODO(thakis): Double-clicking a row seems to do something on win/linux
// TODO(thakis): On window close, stop updating
// TODO(thakis): Favicons in rows
// TODO(thakis): Default sort column
// TODO(thakis): Metrics are all wrong (some fixed when about:memory lands?)

@interface TaskManagerWindowController (Private)
- (void)addColumnWithId:(int)columnId visible:(BOOL)isVisible;
- (void)setUpTableColumns;
- (void)setUpTableHeaderContextMenu;
- (void)toggleColumn:(id)sender;
@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWindowController implementation:

@implementation TaskManagerWindowController

- (id)initWithModel:(TaskManagerModel*)model {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"TaskManager"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    model_ = model;
    [[self window] makeKeyAndOrderFront:self];
  }
  return self;
}

- (void)reloadData {
  [tableView_ reloadData];
}

- (void)awakeFromNib {
  [self setUpTableColumns];
  [self setUpTableHeaderContextMenu];
}

// Adds a column which has the given string id as title. |isVisible| specifies
// if the column is initially visible.
- (void)addColumnWithId:(int)columnId visible:(BOOL)isVisible {
  scoped_nsobject<NSTableColumn> column([[NSTableColumn alloc]
      initWithIdentifier:[NSNumber numberWithInt:columnId]]);

  NSTextAlignment textAlignment = columnId == IDS_TASK_MANAGER_PAGE_COLUMN ?
      NSLeftTextAlignment : NSRightTextAlignment; 

  [[column.get() headerCell]
      setStringValue:l10n_util::GetNSStringWithFixup(columnId)];
  [[column.get() headerCell] setAlignment:textAlignment];
  [[column.get() dataCell] setAlignment:textAlignment];

  [column.get() setHidden:!isVisible];
  [column.get() setEditable:NO];
  [tableView_ addTableColumn:column.get()];
}

// Adds all the task manager's columns to the table.
- (void)setUpTableColumns {
  for (NSTableColumn* column in [tableView_ tableColumns])
    [tableView_ removeTableColumn:column];
  [self addColumnWithId:IDS_TASK_MANAGER_PAGE_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_SHARED_MEM_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_CPU_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_NET_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_PROCESS_ID_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN visible:NO];
}

// Creates a context menu for the table header that allows the user to toggle
// which columns should be shown and which should be hidden (like e.g.
// Task Manager.app's table header context menu).
- (void)setUpTableHeaderContextMenu {
  scoped_nsobject<NSMenu> contextMenu(
      [[NSMenu alloc] initWithTitle:@"Task Manager context menu"]);
  for (NSTableColumn* column in [tableView_ tableColumns]) {
    NSMenuItem* item = [contextMenu.get()
        addItemWithTitle:[[column headerCell] stringValue]
                  action:@selector(toggleColumn:)
           keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:column];
    [item setState:[column isHidden] ? NSOffState : NSOnState];
  }
  [[tableView_ headerView] setMenu:contextMenu.get()];
}

// Callback for the table header context menu. Toggles visibility of the table
// column associated with the clicked menu item.
- (void)toggleColumn:(id)item {
  DCHECK([item isKindOfClass:[NSMenuItem class]]);
  if (![item isKindOfClass:[NSMenuItem class]])
    return;

  NSTableColumn* column = [item representedObject];
  DCHECK(column);
  NSInteger oldState = [item state];
  NSInteger newState = oldState == NSOnState ? NSOffState : NSOnState;
  [column setHidden:newState == NSOffState];
  [item setState:newState];
  [tableView_ sizeToFit];
  [tableView_ setNeedsDisplay];  
}

@end

@implementation TaskManagerWindowController (NSTableDataSource)

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  DCHECK(tableView == tableView_ || tableView_ == nil);
  return model_->ResourceCount();
}

- (NSString*)modelTextForRow:(int)row column:(int)columnId {
  switch (columnId) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return base::SysWideToNSString(model_->GetResourceTitle(row));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(model_->GetResourcePrivateMemory(row));

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(model_->GetResourceSharedMemory(row));

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(model_->GetResourcePhysicalMemory(row));

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(model_->GetResourceCPUUsage(row));

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return base::SysWideToNSString(model_->GetResourceNetworkUsage(row));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:  // Process ID
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(model_->GetResourceProcessId(row));

    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:  // Goats Teleported!
      return base::SysWideToNSString(model_->GetResourceGoatsTeleported(row));

    default:
      return base::SysWideToNSString(
          model_->GetResourceStatsValue(row, columnId));
  }
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  return [self modelTextForRow:rowIndex
                        column:[[tableColumn identifier] intValue]];
}

@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac implementation:

TaskManagerMac::TaskManagerMac()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()) {
  window_controller_.reset(
      [[TaskManagerWindowController alloc] initWithModel:model_]);
  model_->AddObserver(this);
}

// static
TaskManagerMac* TaskManagerMac::instance_ = NULL;

TaskManagerMac::~TaskManagerMac() {
  task_manager_->OnWindowClosed();
  model_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac, TaskManagerModelObserver implementation:

void TaskManagerMac::OnModelChanged() {
  [window_controller_.get() reloadData];
}

void TaskManagerMac::OnItemsChanged(int start, int length) {
  [window_controller_.get() reloadData];
}

void TaskManagerMac::OnItemsAdded(int start, int length) {
  [window_controller_.get() reloadData];
}

void TaskManagerMac::OnItemsRemoved(int start, int length) {
  [window_controller_.get() reloadData];
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac, public:

// static
void TaskManagerMac::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    [[instance_->window_controller_ window]
        makeKeyAndOrderFront:instance_->window_controller_];
  } else {
    instance_ = new TaskManagerMac;
    instance_->model_->StartUpdating();
  }
}
