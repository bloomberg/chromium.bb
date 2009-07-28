// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"

class BaseDownloadItemModel;
@class DownloadItemCell;
class DownloadItemMac;
class DownloadShelfContextMenuMac;
@class DownloadShelfController;

// A controller class that manages one download item.

@interface DownloadItemController : NSViewController {
 @private
  IBOutlet NSButton* progressView_;
  IBOutlet DownloadItemCell* cell_;

  IBOutlet NSMenu* activeDownloadMenu_;
  IBOutlet NSMenu* completeDownloadMenu_;

  NSMenu* currentMenu_;  // points to one of the two menus above

  scoped_ptr<DownloadItemMac> bridge_;
  scoped_ptr<DownloadShelfContextMenuMac> menuBridge_;

  // Weak pointer to the shelf that owns us.
  DownloadShelfController* shelf_;
};

// Takes ownership of |downloadModel|.
- (id)initWithFrame:(NSRect)frameRect
              model:(BaseDownloadItemModel*)downloadModel
              shelf:(DownloadShelfController*)shelf;

// Updates the UI and menu state from |downloadModel|.
- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel;

// Remove ourself from the download UI.
- (void)remove;

// Update item's visibility depending on if the item is still completely
// contained in its parent.
- (void)updateVisibility:(id)sender;

// Asynchronous icon loading callback.
- (void)setIcon:(NSImage*)icon;

// Download item button clicked
- (IBAction)handleButtonClick:(id)sender;

// Context menu handlers.
- (IBAction)handleOpen:(id)sender;
- (IBAction)handleAlwaysOpen:(id)sender;
- (IBAction)handleReveal:(id)sender;
- (IBAction)handleCancel:(id)sender;

@end

