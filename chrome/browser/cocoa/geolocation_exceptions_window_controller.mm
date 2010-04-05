// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/geolocation_exceptions_window_controller.h"

#include <set>

#include "app/l10n_util_mac.h"
#include "app/table_model_observer.h"
#import "base/mac_util.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_content_settings_table_model.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

@interface GeolocationExceptionsWindowController (Private)
- (id)initWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap;
- (void)selectedRemovableIndices:(std::set<int>*)indices;
- (int)countSelectedRemovable;
- (void)adjustEditingButtons;
- (void)modelDidChange;
@end

// Observer for the geolocation table model.
class GeolocationObserverBridge : public TableModelObserver {
 public:
  GeolocationObserverBridge(GeolocationExceptionsWindowController* controller)
      : controller_(controller) {}
  virtual ~GeolocationObserverBridge() {}

  virtual void OnModelChanged() {
    [controller_ modelDidChange];
  }
  virtual void OnItemsChanged(int start, int length) {
    [controller_ modelDidChange];
  }
  virtual void OnItemsAdded(int start, int length) {
    [controller_ modelDidChange];
  }
  virtual void OnItemsRemoved(int start, int length) {
    [controller_ modelDidChange];
  }

 private:
  GeolocationExceptionsWindowController* controller_;  // weak
};

namespace  {

const CGFloat kButtonBarHeight = 35.0;

GeolocationExceptionsWindowController* g_exceptionWindow = nil;

}  // namespace

@implementation GeolocationExceptionsWindowController

+ (id)showWindowWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap {
  if (!g_exceptionWindow) {
    g_exceptionWindow = [[GeolocationExceptionsWindowController alloc]
        initWithSettingsMap:settingsMap];
  }
  [g_exceptionWindow showWindow:nil];
  return g_exceptionWindow;
}

- (id)initWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap {
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"GeolocationExceptionsWindow"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    settingsMap_ = settingsMap;
    model_.reset(new GeolocationContentSettingsTableModel(settingsMap_));
    tableObserver_.reset(new GeolocationObserverBridge(self));
    model_->SetObserver(tableObserver_.get());

    // TODO(thakis): autoremember window rect.
    // TODO(thakis): sorting support.
  }
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);
  DCHECK(tableView_);
  DCHECK_EQ(self, [tableView_ dataSource]);
  DCHECK_EQ(self, [tableView_ delegate]);

  // Make sure the button fits its label, but keep it the same height as the
  // other two buttons.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:removeAllButton_];
  NSSize size = [removeAllButton_ frame].size;
  size.height = NSHeight([removeButton_ frame]);
  [removeAllButton_ setFrameSize:size];

  [self adjustEditingButtons];

  // Give the button bar on the bottom of the window the "iTunes/iChat" look.
  [[self window] setAutorecalculatesContentBorderThickness:NO
                                                   forEdge:NSMinYEdge];
  [[self window] setContentBorderThickness:kButtonBarHeight
                                   forEdge:NSMinYEdge];
}

- (void)windowWillClose:(NSNotification*)notification {
  // Without this, some of the unit tests fail on 10.6:
  [tableView_ setDataSource:nil];

  g_exceptionWindow = nil;
  [self autorelease];
}

// Let esc close the window.
- (void)cancel:(id)sender {
  [self close];
}

- (void)keyDown:(NSEvent*)event {
  NSString* chars = [event charactersIgnoringModifiers];
  if ([chars length] == 1) {
    switch ([chars characterAtIndex:0]) {
      case NSDeleteCharacter:
      case NSDeleteFunctionKey:
        // Delete deletes.
        if ([[tableView_ selectedRowIndexes] count] > 0)
          [self removeException:self];
        return;
    }
  }
  [super keyDown:event];
}

- (IBAction)removeException:(id)sender {
  std::set<int> indices;
  [self selectedRemovableIndices:&indices];

  for (std::set<int>::reverse_iterator i = indices.rbegin();
       i != indices.rend(); ++i) {
    model_->RemoveException(*i);
  }
}

- (IBAction)removeAllExceptions:(id)sender {
  model_->RemoveAll();
}

// Table View Data Source -----------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table {
  return model_->RowCount();
}

- (id)tableView:(NSTableView*)tv
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)row {
  NSObject* result = nil;
  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"hostname"]) {
    std::wstring host = model_->GetText(row, IDS_EXCEPTIONS_HOSTNAME_HEADER);
    result = base::SysWideToNSString(host);
  } else if ([identifier isEqualToString:@"action"]) {
    std::wstring action = model_->GetText(row, IDS_EXCEPTIONS_ACTION_HEADER);
    result = base::SysWideToNSString(action);
  } else {
    NOTREACHED();
  }
  return result;
}

// Table View Delegate --------------------------------------------------------

// When the selection in the table view changes, we need to adjust buttons.
- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  [self adjustEditingButtons];
}

// Private --------------------------------------------------------------------

// Returns the indices of all selected rows that are removable.
- (void)selectedRemovableIndices:(std::set<int>*)indices {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  for (NSUInteger index = [selection lastIndex]; index != NSNotFound;
       index = [selection indexLessThanIndex:index]) {
    if (model_->CanRemoveException(index))
      indices->insert(index);
  }
}

// Returns how many of the selected rows are removable.
- (int)countSelectedRemovable {
  std::set<int> indices;
  [self selectedRemovableIndices:&indices];
  return indices.size();
}

// This method appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustEditingButtons {
  [removeButton_ setEnabled:([self countSelectedRemovable] > 0)];
  [removeAllButton_ setEnabled:([tableView_ numberOfRows] > 0)];
}

- (void)modelDidChange {
  [tableView_ reloadData];
  [self adjustEditingButtons];
}

@end
