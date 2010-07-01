// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"

class GeolocationExceptionsTableModel;
class GeolocationObserverBridge;

// Controller for the geolocation exception dialog.
@interface GeolocationExceptionsWindowController : NSWindowController
                                                   <NSWindowDelegate,
                                                    NSTableViewDataSource,
                                                    NSTableViewDelegate> {
 @private
  IBOutlet NSTableView* tableView_;
  IBOutlet NSButton* removeButton_;
  IBOutlet NSButton* removeAllButton_;
  IBOutlet NSButton* doneButton_;

  GeolocationContentSettingsMap* settingsMap_;  // weak
  scoped_ptr<GeolocationExceptionsTableModel> model_;
  scoped_ptr<GeolocationObserverBridge> tableObserver_;
}

// Shows or makes frontmost the geolocation exceptions window.
// Changes made by the user in the window are persisted in |settingsMap|.
+ (id)controllerWithSettingsMap:(GeolocationContentSettingsMap*)settingsMap;

// Sets the minimum width of the sheet and resizes it if necessary.
- (void)setMinWidth:(CGFloat)minWidth;

- (void)attachSheetTo:(NSWindow*)window;
- (IBAction)closeSheet:(id)sender;

- (IBAction)removeRow:(id)sender;
- (IBAction)removeAll:(id)sender;

@end
