// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/common/content_settings_types.h"

class ContentExceptionsTableModel;
class ContentSettingComboModel;
class UpdatingContentSettingsObserver;

// Controller for the content exception dialogs.
@interface ContentExceptionsWindowController : NSWindowController
                                               <NSWindowDelegate,
                                                NSTableViewDataSource,
                                                NSTableViewDelegate> {
 @private
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* addButton_;
  IBOutlet NSButton* removeButton_;
  IBOutlet NSButton* removeAllButton_;
  IBOutlet NSButton* doneButton_;

  ContentSettingsType settingsType_;
  HostContentSettingsMap* settingsMap_;  // weak
  HostContentSettingsMap* otrSettingsMap_;  // weak
  scoped_ptr<ContentExceptionsTableModel> model_;
  scoped_ptr<ContentSettingComboModel> popup_model_;

  // Is set if adding and editing exceptions for the current OTR session should
  // be allowed.
  BOOL otrAllowed_;

  // Listens for changes to the content settings and reloads the data when they
  // change. See comment in -modelDidChange in the mm file for details.
  scoped_ptr<UpdatingContentSettingsObserver> tableObserver_;

  // If this is set to NO, notifications by |tableObserver_| are ignored. This
  // is used to suppress updates at bad times.
  BOOL updatesEnabled_;

  // This is non-NULL only while a new element is being added and its pattern
  // is being edited.
  scoped_ptr<HostContentSettingsMap::PatternSettingPair> newException_;
}

// Returns the content exceptions window controller for |settingsType|.
// Changes made by the user in the window are persisted in |settingsMap|.
+ (id)controllerForType:(ContentSettingsType)settingsType
            settingsMap:(HostContentSettingsMap*)settingsMap
         otrSettingsMap:(HostContentSettingsMap*)otrSettingsMap;

// Shows the exceptions dialog as a modal sheet attached to |window|.
- (void)attachSheetTo:(NSWindow*)window;

// Sets the minimum width of the sheet and resizes it if necessary.
- (void)setMinWidth:(CGFloat)minWidth;

- (IBAction)addException:(id)sender;
- (IBAction)removeException:(id)sender;
- (IBAction)removeAllExceptions:(id)sender;
// Closes the sheet and ends the modal loop.
- (IBAction)closeSheet:(id)sender;

@end

@interface ContentExceptionsWindowController(VisibleForTesting)
- (void)cancel:(id)sender;
- (BOOL)editingNewException;
@end
