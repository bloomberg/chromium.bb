// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/task_manager_mac.h"

#include <algorithm>
#include <vector>

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/ui/cocoa/window_size_autosaver.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Width of "a" and most other letters/digits in "small" table views.
const int kCharWidth = 6;

// Some of the strings below have spaces at the end or are missing letters, to
// make the columns look nicer, and to take potentially longer localized strings
// into account.
const struct ColumnWidth {
  int columnId;
  int minWidth;
  int maxWidth;  // If this is -1, 1.5*minColumWidth is used as max width.
} columnWidths[] = {
  // Note that arraysize includes the trailing \0. That's intended.
  { IDS_TASK_MANAGER_PAGE_COLUMN, 120, 600 },
  { IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN,
      arraysize("800 MiB") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_SHARED_MEM_COLUMN,
      arraysize("800 MiB") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN,
      arraysize("800 MiB") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_CPU_COLUMN,
      arraysize("99.9") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_NET_COLUMN,
      arraysize("150 kiB/s") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_PROCESS_ID_COLUMN,
      arraysize("73099  ") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN,
      arraysize("2000.0K (2000.0 live)") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN,
      arraysize("2000.0K (2000.0 live)") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN,
      arraysize("2000.0K (2000.0 live)") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN,
      arraysize("800 kB") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN,
      arraysize("2000.0K (2000.0 live)") * kCharWidth, -1 },
  { IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN,
      arraysize("15 ") * kCharWidth, -1 },
};

class SortHelper {
 public:
  SortHelper(TaskManagerModel* model, NSSortDescriptor* column)
      : sort_column_([[column key] intValue]),
        ascending_([column ascending]),
        model_(model) {}

  bool operator()(int a, int b) {
    std::pair<int, int> group_range1 = model_->GetGroupRangeForResource(a);
    std::pair<int, int> group_range2 = model_->GetGroupRangeForResource(b);
    if (group_range1 == group_range2) {
      // The two rows are in the same group, sort so that items in the same
      // group always appear in the same order. |ascending_| is intentionally
      // ignored.
      return a < b;
    }
    // Sort by the first entry of each of the groups.
    int cmp_result = model_->CompareValues(
        group_range1.first, group_range2.first, sort_column_);
    if (!ascending_)
      cmp_result = -cmp_result;
    return cmp_result < 0;
  }
 private:
  int sort_column_;
  bool ascending_;
  TaskManagerModel* model_;  // weak;
};

}  // namespace

@interface TaskManagerWindowController (Private)
- (NSTableColumn*)addColumnWithId:(int)columnId visible:(BOOL)isVisible;
- (void)setUpTableColumns;
- (void)setUpTableHeaderContextMenu;
- (void)toggleColumn:(id)sender;
- (void)adjustSelectionAndEndProcessButton;
- (void)deselectRows;
@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWindowController implementation:

@implementation TaskManagerWindowController

- (id)initWithTaskManagerObserver:(TaskManagerMac*)taskManagerObserver {
  NSString* nibpath = [base::mac::MainAppBundle()
                        pathForResource:@"TaskManager"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    taskManagerObserver_ = taskManagerObserver;
    taskManager_ = taskManagerObserver_->task_manager();
    model_ = taskManager_->model();

    if (g_browser_process && g_browser_process->local_state()) {
      size_saver_.reset([[WindowSizeAutosaver alloc]
          initWithWindow:[self window]
             prefService:g_browser_process->local_state()
                    path:prefs::kTaskManagerWindowPlacement]);
    }
    [self showWindow:self];
  }
  return self;
}

- (void)sortShuffleArray {
  viewToModelMap_.resize(model_->ResourceCount());
  for (size_t i = 0; i < viewToModelMap_.size(); ++i)
    viewToModelMap_[i] = i;

  std::sort(viewToModelMap_.begin(), viewToModelMap_.end(),
            SortHelper(model_, currentSortDescriptor_.get()));

  modelToViewMap_.resize(viewToModelMap_.size());
  for (size_t i = 0; i < viewToModelMap_.size(); ++i)
    modelToViewMap_[viewToModelMap_[i]] = i;
}

- (void)reloadData {
  // Store old view indices, and the model indices they map to.
  NSIndexSet* viewSelection = [tableView_ selectedRowIndexes];
  std::vector<int> modelSelection;
  for (NSUInteger i = [viewSelection lastIndex];
       i != NSNotFound;
       i = [viewSelection indexLessThanIndex:i]) {
    modelSelection.push_back(viewToModelMap_[i]);
  }

  // Sort.
  [self sortShuffleArray];

  // Use the model indices to get the new view indices of the selection, and
  // set selection to that. This assumes that no rows were added or removed
  // (in that case, the selection is cleared before -reloadData is called).
  if (modelSelection.size() > 0)
    DCHECK_EQ([tableView_ numberOfRows], model_->ResourceCount());
  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  for (size_t i = 0; i < modelSelection.size(); ++i)
    [indexSet addIndex:modelToViewMap_[modelSelection[i]]];
  [tableView_ selectRowIndexes:indexSet byExtendingSelection:NO];

  [tableView_ reloadData];
  [self adjustSelectionAndEndProcessButton];
}

- (IBAction)statsLinkClicked:(id)sender {
  TaskManager::GetInstance()->OpenAboutMemory();
}

- (IBAction)killSelectedProcesses:(id)sender {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    taskManager_->KillProcess(viewToModelMap_[i]);
  }
}

- (void)selectDoubleClickedTab:(id)sender {
  NSInteger row = [tableView_ clickedRow];
  if (row < 0)
    return;  // Happens e.g. if the table header is double-clicked.
  taskManager_->ActivateProcess(viewToModelMap_[row]);
}

- (NSTableView*)tableView {
  return tableView_;
}

- (void)awakeFromNib {
  [self setUpTableColumns];
  [self setUpTableHeaderContextMenu];
  [self adjustSelectionAndEndProcessButton];

  [tableView_ setDoubleAction:@selector(selectDoubleClickedTab:)];
  [tableView_ sizeToFit];
}

- (void)dealloc {
  [tableView_ setDelegate:nil];
  [tableView_ setDataSource:nil];
  [super dealloc];
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

  // The page column should by default be sorted ascending.
  BOOL ascending = columnId == IDS_TASK_MANAGER_PAGE_COLUMN;

  scoped_nsobject<NSSortDescriptor> sortDescriptor([[NSSortDescriptor alloc]
      initWithKey:[NSString stringWithFormat:@"%d", columnId]
        ascending:ascending]);
  [column.get() setSortDescriptorPrototype:sortDescriptor.get()];

  // Default values, only used in release builds if nobody notices the DCHECK
  // during development when adding new columns.
  int minWidth = 200, maxWidth = 400;

  size_t i;
  for (i = 0; i < arraysize(columnWidths); ++i) {
    if (columnWidths[i].columnId == columnId) {
      minWidth = columnWidths[i].minWidth;
      maxWidth = columnWidths[i].maxWidth;
      if (maxWidth < 0)
        maxWidth = 3 * minWidth / 2;  // *1.5 for ints.
      break;
    }
  }
  DCHECK(i < arraysize(columnWidths)) << "Could not find " << columnId;
  [column.get() setMinWidth:minWidth];
  [column.get() setMaxWidth:maxWidth];
  [column.get() setResizingMask:NSTableColumnAutoresizingMask |
                                NSTableColumnUserResizingMask];

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

  // Initially, sort on the tab name.
  [tableView_ setSortDescriptors:
      [NSArray arrayWithObject:[nameColumn sortDescriptorPrototype]]];

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
  [self addColumnWithId:IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN visible:NO];
  [self addColumnWithId:IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN
                visible:NO];
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
- (void)adjustSelectionAndEndProcessButton {
  bool selectionContainsBrowserProcess = false;

  // If a row is selected, make sure that all rows belonging to the same process
  // are selected as well. Also, check if the selection contains the browser
  // process.
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  for (NSUInteger i = [selection lastIndex];
       i != NSNotFound;
       i = [selection indexLessThanIndex:i]) {
    int modelIndex = viewToModelMap_[i];
    if (taskManager_->IsBrowserProcess(modelIndex))
      selectionContainsBrowserProcess = true;

    std::pair<int, int> rangePair =
        model_->GetGroupRangeForResource(modelIndex);
    NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
    for (int j = 0; j < rangePair.second; ++j)
      [indexSet addIndex:modelToViewMap_[rangePair.first + j]];
    [tableView_ selectRowIndexes:indexSet byExtendingSelection:YES];
  }

  bool enabled = [selection count] > 0 && !selectionContainsBrowserProcess;
  [endProcessButton_ setEnabled:enabled];
}

- (void)deselectRows {
  [tableView_ deselectAll:self];
}

// Table view delegate method.
- (void)tableViewSelectionIsChanging:(NSNotification*)aNotification {
  [self adjustSelectionAndEndProcessButton];
}

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
  DCHECK_LT(static_cast<size_t>(row), viewToModelMap_.size());
  row = viewToModelMap_[row];
  switch (columnId) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return base::SysUTF16ToNSString(model_->GetResourceTitle(row));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(model_->GetResourcePrivateMemory(row));

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(model_->GetResourceSharedMemory(row));

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(model_->GetResourcePhysicalMemory(row));

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(model_->GetResourceCPUUsage(row));

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return base::SysUTF16ToNSString(model_->GetResourceNetworkUsage(row));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:  // Process ID
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(model_->GetResourceProcessId(row));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:  // WebCore image cache
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(
          model_->GetResourceWebCoreImageCacheSize(row));

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:  // WebCore script cache
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(
          model_->GetResourceWebCoreScriptsCacheSize(row));

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:  // WebCore CSS cache
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(
          model_->GetResourceWebCoreCSSCacheSize(row));

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(
          model_->GetResourceSqliteMemoryUsed(row));

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return @"";
      return base::SysUTF16ToNSString(
          model_->GetResourceV8MemoryAllocatedSize(row));

    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:  // Goats Teleported!
      return base::SysUTF16ToNSString(model_->GetResourceGoatsTeleported(row));

    default:
      NOTREACHED();
      return @"";
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

  // Set the favicon and title for the task in the name column.
  if ([[tableColumn identifier] intValue] == IDS_TASK_MANAGER_PAGE_COLUMN) {
    DCHECK([cell isKindOfClass:[NSButtonCell class]]);
    NSButtonCell* buttonCell = static_cast<NSButtonCell*>(cell);
    NSString* title = [self modelTextForRow:rowIndex
                                    column:[[tableColumn identifier] intValue]];
    [buttonCell setTitle:title];
    [buttonCell setImage:
        taskManagerObserver_->GetImageForRow(viewToModelMap_[rowIndex])];
    [buttonCell setRefusesFirstResponder:YES];  // Don't push in like a button.
    [buttonCell setHighlightsBy:NSNoCellMask];
  }

  return cell;
}

- (void)           tableView:(NSTableView*)tableView
    sortDescriptorsDidChange:(NSArray*)oldDescriptors {
  NSArray* newDescriptors = [tableView sortDescriptors];
  if ([newDescriptors count] < 1)
    return;

  currentSortDescriptor_.reset([[newDescriptors objectAtIndex:0] retain]);
  [self reloadData];  // Sorts.
}

@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac implementation:

TaskManagerMac::TaskManagerMac(TaskManager* task_manager)
  : task_manager_(task_manager),
    model_(task_manager->model()),
    icon_cache_(this) {
  window_controller_ =
      [[TaskManagerWindowController alloc] initWithTaskManagerObserver:this];
  model_->AddObserver(this);
}

// static
TaskManagerMac* TaskManagerMac::instance_ = NULL;

TaskManagerMac::~TaskManagerMac() {
  if (this == instance_) {
    // Do not do this when running in unit tests: |StartUpdating()| never got
    // called in that case.
    task_manager_->OnWindowClosed();
  }
  model_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac, TaskManagerModelObserver implementation:

void TaskManagerMac::OnModelChanged() {
  icon_cache_.OnModelChanged();
  [window_controller_ deselectRows];
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsChanged(int start, int length) {
  icon_cache_.OnItemsChanged(start, length);
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsAdded(int start, int length) {
  icon_cache_.OnItemsAdded(start, length);
  [window_controller_ deselectRows];
  [window_controller_ reloadData];
}

void TaskManagerMac::OnItemsRemoved(int start, int length) {
  icon_cache_.OnItemsRemoved(start, length);
  [window_controller_ deselectRows];
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

int TaskManagerMac::RowCount() const {
  return model_->ResourceCount();
}

SkBitmap TaskManagerMac::GetIcon(int r) const {
  return model_->GetResourceIcon(r);
}

// static
void TaskManagerMac::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    [[instance_->window_controller_ window]
        makeKeyAndOrderFront:instance_->window_controller_];
  } else {
    instance_ = new TaskManagerMac(TaskManager::GetInstance());
    instance_->model_->StartUpdating();
  }
}
