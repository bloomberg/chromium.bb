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
// TODO(thakis): Better resizing behavior (and think about storing column sizes)
// TODO(thakis): Column sort comparator
// TODO(thakis): Clicking column header doesn't sort
// TODO(thakis): Default sort column

@interface TaskManagerWindowController (Private)
- (NSTableColumn*)addColumnWithId:(int)columnId visible:(BOOL)isVisible;
- (void)setUpTableColumns;
- (void)setUpTableHeaderContextMenu;
- (void)toggleColumn:(id)sender;
- (void)adjustEndProcessButton;
@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWindowController implementation:

@implementation TaskManagerWindowController

- (id)initWithTaskManagerObserver:(TaskManagerMac*)taskManagerObserver {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"TaskManager"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    taskManagerObserver_ = taskManagerObserver;
    taskManager_ = taskManagerObserver_->task_manager();
    model_ = taskManager_->model();
    [[self window] makeKeyAndOrderFront:self];
  }
  return self;
}

- (void)reloadData {
  [tableView_ reloadData];
  [self adjustEndProcessButton];
}

- (IBAction)statsLinkClicked:(id)sender {
  TaskManager::GetInstance()->OpenAboutMemory();
}

- (IBAction)killSelectedProcesses:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    taskManager_->KillProcess(i);
  }
}

- (void)selectDoubleClickedTab:(id)sender {
  NSInteger row = [tableView_ clickedRow];
  if (row < 0)
    return;  // Happens e.g. if the table header is double-clicked.
  taskManager_->ActivateProcess(row);
}

- (void)awakeFromNib {
  [self setUpTableColumns];
  [self setUpTableHeaderContextMenu];
  [self adjustEndProcessButton];

  [tableView_ setDoubleAction:@selector(selectDoubleClickedTab:)];
}

// Adds a column which has the given string id as title. |isVisible| specifies
// if the column is initially visible.
- (NSTableColumn*)addColumnWithId:(int)columnId visible:(BOOL)isVisible {
  scoped_nsobject<NSTableColumn> column([[NSTableColumn alloc]
      initWithIdentifier:[NSNumber numberWithInt:columnId]]);

  NSTextAlignment textAlignment = columnId == IDS_TASK_MANAGER_PAGE_COLUMN ?
      NSLeftTextAlignment : NSRightTextAlignment;

  [[column.get() headerCell]
      setStringValue:l10n_util::GetNSStringWithFixup(columnId)];
  [[column.get() headerCell] setAlignment:textAlignment];
  [[column.get() dataCell] setAlignment:textAlignment];

  NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
  [[column.get() dataCell] setFont:font];
  
  [column.get() setHidden:!isVisible];
  [column.get() setEditable:NO];
  [tableView_ addTableColumn:column.get()];
  return column.get();  // Now retained by |tableView_|.
}

// Adds all the task manager's columns to the table.
- (void)setUpTableColumns {
  for (NSTableColumn* column in [tableView_ tableColumns])
    [tableView_ removeTableColumn:column];
  NSTableColumn* nameColumn = [self addColumnWithId:IDS_TASK_MANAGER_PAGE_COLUMN
                                            visible:YES];
  // |nameColumn| displays an icon for every row -- this is done by an
  // NSButtonCell.
  scoped_nsobject<NSButtonCell> nameCell(
      [[NSButtonCell alloc] initTextCell:@""]);
  [nameCell.get() setImagePosition:NSImageLeft];
  [nameCell.get() setButtonType:NSSwitchButton];
  [nameCell.get() setAlignment:[[nameColumn dataCell] alignment]];
  [nameCell.get() setFont:[[nameColumn dataCell] font]];
  [nameColumn setDataCell:nameCell.get()];

  [self addColumnWithId:IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_SHARED_MEM_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_CPU_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_NET_COLUMN visible:YES];
  [self addColumnWithId:IDS_TASK_MANAGER_PROCESS_ID_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN
                visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN
                visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN visible:NO];
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

// This function appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustEndProcessButton {
  bool selectionContainsBrowserProcess = false;

  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    if (taskManager_->IsBrowserProcess(i)) {
      selectionContainsBrowserProcess = true;
      break;
    }
  }

  bool enabled = [selection count] > 0 && !selectionContainsBrowserProcess;
  [endProcessButton_ setEnabled:enabled];
}

// Table view delegate method.
- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [self adjustEndProcessButton];
}

// Called when the window is being closed. Send out a notification that the user
// is done editing preferences. Make sure there are no pending field editors
// by clearing the first responder.
- (void)windowWillClose:(NSNotification*)notification {
  if (taskManagerObserver_) {
    taskManagerObserver_->WindowWasClosed();
    taskManagerObserver_ = nil;
  }
  [self autorelease];
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

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:  // WebCore image cache
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(
          model_->GetResourceWebCoreImageCacheSize(row));

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:  // WebCore script cache
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(
          model_->GetResourceWebCoreScriptsCacheSize(row));

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:  // WebCore CSS cache
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysWideToNSString(
          model_->GetResourceWebCoreCSSCacheSize(row));

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
  // NSButtonCells expect an on/off state as objectValue. Their title is set
  // in |tableView:dataCellForTableColumn:row:| below.
  if ([[tableColumn identifier] intValue] == IDS_TASK_MANAGER_PAGE_COLUMN) {
    return [NSNumber numberWithInt:NSOffState];
  }

  return [self modelTextForRow:rowIndex
                        column:[[tableColumn identifier] intValue]];
}

- (NSCell*)tableView:(NSTableView*)tableView
    dataCellForTableColumn:(NSTableColumn*)tableColumn
                       row:(NSInteger)rowIndex {
  NSCell* cell = [tableColumn dataCellForRow:rowIndex];

  // Set the favicon and title for the search engine in the name column.
  if ([[tableColumn identifier] intValue] == IDS_TASK_MANAGER_PAGE_COLUMN) {
    DCHECK([cell isKindOfClass:[NSButtonCell class]]);
    NSButtonCell* buttonCell = static_cast<NSButtonCell*>(cell);
    NSString* title = [self modelTextForRow:rowIndex
                                    column:[[tableColumn identifier] intValue]];
    [buttonCell setTitle:title];
    [buttonCell setImage:taskManagerObserver_->GetImageForRow(rowIndex)];
    [buttonCell setRefusesFirstResponder:YES];  // Don't push in like a button.
    [buttonCell setHighlightsBy:NSNoCellMask];
  }

  return cell;
}

@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac implementation:

TaskManagerMac::TaskManagerMac()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()),
    icon_cache_(this) {
  window_controller_ =
      [[TaskManagerWindowController alloc] initWithTaskManagerObserver:this];
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
  icon_cache_.OnModelChanged();
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsChanged(int start, int length) {
  icon_cache_.OnItemsChanged(start, length);
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsAdded(int start, int length) {
  icon_cache_.OnItemsAdded(start, length);
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsRemoved(int start, int length) {
  icon_cache_.OnItemsRemoved(start, length);
  [window_controller_ reloadData];
}

NSImage* TaskManagerMac::GetImageForRow(int row) {
  return icon_cache_.GetImageForRow(row);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac, public:

void TaskManagerMac::WindowWasClosed() {
  delete this;
  instance_ = NULL;
}

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
