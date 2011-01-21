// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/options/content_exceptions_window_controller.h"

#include "base/command_line.h"
#import "base/mac/mac_util.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/content_setting_combo_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/table_model_observer.h"

@interface ContentExceptionsWindowController (Private)
- (id)initWithType:(ContentSettingsType)settingsType
       settingsMap:(HostContentSettingsMap*)settingsMap
    otrSettingsMap:(HostContentSettingsMap*)otrSettingsMap;
- (void)updateRow:(NSInteger)row
        withEntry:(const HostContentSettingsMap::PatternSettingPair&)entry
           forOtr:(BOOL)isOtr;
- (void)adjustEditingButtons;
- (void)modelDidChange;
- (NSString*)titleForIndex:(size_t)index;
@end

////////////////////////////////////////////////////////////////////////////////
// PatternFormatter

// A simple formatter that accepts text that vaguely looks like a pattern.
@interface PatternFormatter : NSFormatter
@end

@implementation PatternFormatter
- (NSString*)stringForObjectValue:(id)object {
  if (![object isKindOfClass:[NSString class]])
    return nil;
  return object;
}

- (BOOL)getObjectValue:(id*)object
             forString:(NSString*)string
      errorDescription:(NSString**)error {
  if ([string length]) {
      if (ContentSettingsPattern(
          base::SysNSStringToUTF8(string)).IsValid()) {
      *object = string;
      return YES;
    }
  }
  if (error)
    *error = @"Invalid pattern";
  return NO;
}

- (NSAttributedString*)attributedStringForObjectValue:(id)object
                                withDefaultAttributes:(NSDictionary*)attribs {
  return nil;
}
@end

////////////////////////////////////////////////////////////////////////////////
// UpdatingContentSettingsObserver

// UpdatingContentSettingsObserver is a notification observer that tells a
// window controller to update its data on every notification.
class UpdatingContentSettingsObserver : public NotificationObserver {
 public:
  UpdatingContentSettingsObserver(ContentExceptionsWindowController* controller)
      : controller_(controller) {
    // One would think one could register a TableModelObserver to be notified of
    // changes to ContentExceptionsTableModel. One would be wrong: The table
    // model only sends out changes that are made through the model, not for
    // changes made directly to its backing HostContentSettings object (that
    // happens e.g. if the user uses the cookie confirmation dialog). Hence,
    // observe the CONTENT_SETTINGS_CHANGED notification directly.
    registrar_.Add(this, NotificationType::CONTENT_SETTINGS_CHANGED,
                   NotificationService::AllSources());
  }
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  NotificationRegistrar registrar_;
  ContentExceptionsWindowController* controller_;
};

void UpdatingContentSettingsObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  [controller_ modelDidChange];
}

////////////////////////////////////////////////////////////////////////////////
// Static functions

namespace  {

NSString* GetWindowTitle(ContentSettingsType settingsType) {
  switch (settingsType) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return l10n_util::GetNSStringWithFixup(IDS_COOKIE_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return l10n_util::GetNSStringWithFixup(IDS_IMAGES_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return l10n_util::GetNSStringWithFixup(IDS_JS_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return l10n_util::GetNSStringWithFixup(IDS_PLUGINS_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return l10n_util::GetNSStringWithFixup(IDS_POPUP_EXCEPTION_TITLE);
    default:
      NOTREACHED();
  }
  return @"";
}

const CGFloat kButtonBarHeight = 35.0;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ContentExceptionsWindowController implementation

static ContentExceptionsWindowController*
    g_exceptionWindows[CONTENT_SETTINGS_NUM_TYPES] = { nil };

@implementation ContentExceptionsWindowController

+ (id)controllerForType:(ContentSettingsType)settingsType
            settingsMap:(HostContentSettingsMap*)settingsMap
         otrSettingsMap:(HostContentSettingsMap*)otrSettingsMap {
  if (!g_exceptionWindows[settingsType]) {
    g_exceptionWindows[settingsType] =
        [[ContentExceptionsWindowController alloc]
            initWithType:settingsType
             settingsMap:settingsMap
          otrSettingsMap:otrSettingsMap];
  }
  return g_exceptionWindows[settingsType];
}

- (id)initWithType:(ContentSettingsType)settingsType
       settingsMap:(HostContentSettingsMap*)settingsMap
    otrSettingsMap:(HostContentSettingsMap*)otrSettingsMap {
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"ContentExceptionsWindow"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    settingsType_ = settingsType;
    settingsMap_ = settingsMap;
    otrSettingsMap_ = otrSettingsMap;
    model_.reset(new ContentExceptionsTableModel(
        settingsMap_, otrSettingsMap_, settingsType_));
    popup_model_.reset(new ContentSettingComboModel(settingsType_));
    otrAllowed_ = otrSettingsMap != NULL;
    tableObserver_.reset(new UpdatingContentSettingsObserver(self));
    updatesEnabled_ = YES;

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

  [[self window] setTitle:GetWindowTitle(settingsType_)];

  CGFloat minWidth = [[addButton_ superview] bounds].size.width +
                     [[doneButton_ superview] bounds].size.width;
  [self setMinWidth:minWidth];

  [self adjustEditingButtons];

  // Initialize menu for the data cell in the "action" column.
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"exceptionMenu"]);
  for (int i = 0; i < popup_model_->GetItemCount(); ++i) {
    NSString* title =
        l10n_util::FixUpWindowsStyleLabel(popup_model_->GetItemAt(i));
    scoped_nsobject<NSMenuItem> allowItem(
        [[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""]);
    [allowItem.get() setTag:popup_model_->SettingForIndex(i)];
    [menu.get() addItem:allowItem.get()];
  }
  NSCell* menuCell =
      [[tableView_ tableColumnWithIdentifier:@"action"] dataCell];
  [menuCell setMenu:menu.get()];

  NSCell* patternCell =
      [[tableView_ tableColumnWithIdentifier:@"pattern"] dataCell];
  [patternCell setFormatter:[[[PatternFormatter alloc] init] autorelease]];

  if (!otrAllowed_) {
    [tableView_
        removeTableColumn:[tableView_ tableColumnWithIdentifier:@"otr"]];
  }
}

- (void)setMinWidth:(CGFloat)minWidth {
  NSWindow* window = [self window];
  [window setMinSize:NSMakeSize(minWidth, [window minSize].height)];
  if ([window frame].size.width < minWidth) {
    NSRect frame = [window frame];
    frame.size.width = minWidth;
    [window setFrame:frame display:NO];
  }
}

- (void)windowWillClose:(NSNotification*)notification {
  // Without this, some of the unit tests fail on 10.6:
  [tableView_ setDataSource:nil];

  g_exceptionWindows[settingsType_] = nil;
  [self autorelease];
}

- (BOOL)editingNewException {
  return newException_.get() != NULL;
}

// Let esc cancel editing if the user is currently editing a pattern. Else, let
// esc close the window.
- (void)cancel:(id)sender {
  if ([tableView_ currentEditor] != nil) {
    [tableView_ abortEditing];
    [[self window] makeFirstResponder:tableView_];  // Re-gain focus.

    if ([tableView_ selectedRow] == model_->RowCount()) {
      // Cancel addition of new row.
      [self removeException:self];
    }
  } else {
    [self closeSheet:self];
  }
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
      case NSCarriageReturnCharacter:
      case NSEnterCharacter:
        // Return enters rename mode.
        if ([[tableView_ selectedRowIndexes] count] == 1) {
          [tableView_ editColumn:0
                             row:[[tableView_ selectedRowIndexes] lastIndex]
                       withEvent:nil
                          select:YES];
        }
        return;
    }
  }
  [super keyDown:event];
}

- (void)attachSheetTo:(NSWindow*)window {
  [NSApp beginSheet:[self window]
     modalForWindow:window
      modalDelegate:self
     didEndSelector:@selector(sheetDidEnd:returnCode:contextInfo:)
        contextInfo:nil];
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)context {
  [sheet close];
  [sheet orderOut:self];
}

- (IBAction)addException:(id)sender {
  if (newException_.get()) {
    // The invariant is that |newException_| is non-NULL exactly if the pattern
    // of a new exception is currently being edited - so there's nothing to do
    // in that case.
    return;
  }
  newException_.reset(new HostContentSettingsMap::PatternSettingPair);
  newException_->first = ContentSettingsPattern(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_SAMPLE_PATTERN));
  newException_->second = CONTENT_SETTING_BLOCK;
  [tableView_ reloadData];

  [self adjustEditingButtons];
  int index = model_->RowCount();
  NSIndexSet* selectedSet = [NSIndexSet indexSetWithIndex:index];
  [tableView_ selectRowIndexes:selectedSet byExtendingSelection:NO];
  [tableView_ editColumn:0 row:index withEvent:nil select:YES];
}

- (IBAction)removeException:(id)sender {
  updatesEnabled_ = NO;
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  [tableView_ deselectAll:self];  // Else we'll get a -setObjectValue: later.
  DCHECK_GT([selection count], 0U);
  NSUInteger index = [selection lastIndex];
  while (index != NSNotFound) {
    if (index == static_cast<NSUInteger>(model_->RowCount()))
      newException_.reset();
    else
      model_->RemoveException(index);
    index = [selection indexLessThanIndex:index];
  }
  updatesEnabled_ = YES;
  [self modelDidChange];
}

- (IBAction)removeAllExceptions:(id)sender {
  updatesEnabled_ = NO;
  [tableView_ deselectAll:self];  // Else we'll get a -setObjectValue: later.
  newException_.reset();
  model_->RemoveAll();
  updatesEnabled_ = YES;
  [self modelDidChange];
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}

// Table View Data Source -----------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table {
  return model_->RowCount() + (newException_.get() ? 1 : 0);
}

- (id)tableView:(NSTableView*)tv
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)row {
  const HostContentSettingsMap::PatternSettingPair* entry;
  int isOtr;
  if (newException_.get() && row >= model_->RowCount()) {
    entry = newException_.get();
    isOtr = 0;
  } else {
    entry = &model_->entry_at(row);
    isOtr = model_->entry_is_off_the_record(row) ? 1 : 0;
  }

  NSObject* result = nil;
  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"pattern"]) {
    result = base::SysUTF8ToNSString(entry->first.AsString());
  } else if ([identifier isEqualToString:@"action"]) {
    result =
        [NSNumber numberWithInt:popup_model_->IndexForSetting(entry->second)];
  } else if ([identifier isEqualToString:@"otr"]) {
    result = [NSNumber numberWithInt:isOtr];
  } else {
    NOTREACHED();
  }
  return result;
}

// Updates exception at |row| to contain the data in |entry|.
- (void)updateRow:(NSInteger)row
        withEntry:(const HostContentSettingsMap::PatternSettingPair&)entry
           forOtr:(BOOL)isOtr {
  // TODO(thakis): This apparently moves an edited row to the back of the list.
  // It's what windows and linux do, but it's kinda sucky. Fix.
  // http://crbug.com/36904
  updatesEnabled_ = NO;
  if (row < model_->RowCount())
    model_->RemoveException(row);
  model_->AddException(entry.first, entry.second, isOtr);
  updatesEnabled_ = YES;
  [self modelDidChange];

  // For now, at least re-select the edited element.
  int newIndex = model_->IndexOfExceptionByPattern(entry.first, isOtr);
  DCHECK(newIndex != -1);
  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:newIndex]
          byExtendingSelection:NO];
}

- (void) tableView:(NSTableView*)tv
    setObjectValue:(id)object
    forTableColumn:(NSTableColumn*)tableColumn
               row:(NSInteger)row {
  // -remove: and -removeAll: both call |tableView_|'s -deselectAll:, which
  // calls this method if a cell is currently being edited. Do not commit edits
  // of rows that are about to be deleted.
  if (!updatesEnabled_) {
    // If this method gets called, the pattern filed of the new exception can no
    // longer be being edited. Reset |newException_| to keep the invariant true.
    newException_.reset();
    return;
  }

  // Get model object.
  bool isNewRow = newException_.get() && row >= model_->RowCount();
  HostContentSettingsMap::PatternSettingPair originalEntry =
      isNewRow ? *newException_ : model_->entry_at(row);
  HostContentSettingsMap::PatternSettingPair entry = originalEntry;
  bool isOtr =
      isNewRow ? 0 : model_->entry_is_off_the_record(row);
  bool wasOtr = isOtr;

  // Modify it.
  NSString* identifier = [tableColumn identifier];
  if ([identifier isEqualToString:@"pattern"]) {
    entry.first = ContentSettingsPattern(base::SysNSStringToUTF8(object));
  }
  if ([identifier isEqualToString:@"action"]) {
    int index = [object intValue];
    entry.second = popup_model_->SettingForIndex(index);
  }
  if ([identifier isEqualToString:@"otr"]) {
    isOtr = [object intValue] != 0;
  }

  // Commit modification, if any.
  if (isNewRow) {
    newException_.reset();
    if (![identifier isEqualToString:@"pattern"]) {
      [tableView_ reloadData];
      [self adjustEditingButtons];
      return;  // Commit new rows only when the pattern has been set.
    }
    int newIndex = model_->IndexOfExceptionByPattern(entry.first, false);
    if (newIndex != -1) {
      // The new pattern was already in the table. Focus existing row instead of
      // overwriting it with a new one.
      [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:newIndex]
              byExtendingSelection:NO];
      [tableView_ reloadData];
      [self adjustEditingButtons];
      return;
    }
  }
  if (entry != originalEntry || wasOtr != isOtr || isNewRow)
    [self updateRow:row withEntry:entry forOtr:isOtr];
}


// Table View Delegate --------------------------------------------------------

// When the selection in the table view changes, we need to adjust buttons.
- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  [self adjustEditingButtons];
}

// Private --------------------------------------------------------------------

// This method appropriately sets the enabled states on the table's editing
// buttons.
- (void)adjustEditingButtons {
  NSIndexSet* selection = [tableView_ selectedRowIndexes];
  [removeButton_ setEnabled:([selection count] > 0)];
  [removeAllButton_ setEnabled:([tableView_ numberOfRows] > 0)];
}

- (void)modelDidChange {
  // Some calls on |model_|, e.g. RemoveException(), change something on the
  // backing content settings map object (which sends a notification) and then
  // change more stuff in |model_|. If |model_| is deleted when the notification
  // is sent, this second access causes a segmentation violation. Hence, disable
  // resetting |model_| while updates can be in progress.
  if (!updatesEnabled_)
    return;

  // The model caches its data, meaning we need to recreate it on every change.
  model_.reset(new ContentExceptionsTableModel(
      settingsMap_, otrSettingsMap_, settingsType_));

  [tableView_ reloadData];
  [self adjustEditingButtons];
}

@end
