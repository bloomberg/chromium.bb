// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"

class BaseDownloadItemModel;
@class ChromeUILocalizer;
@class DownloadItemCell;
class DownloadItem;
@class DownloadItemButton;
class DownloadItemMac;
class DownloadShelfContextMenuMac;
@class DownloadShelfController;
@class GTMWidthBasedTweaker;

// A controller class that manages one download item.

@interface DownloadItemController : NSViewController {
 @private
  IBOutlet DownloadItemButton* progressView_;
  IBOutlet DownloadItemCell* cell_;

  IBOutlet NSMenu* activeDownloadMenu_;
  IBOutlet NSMenu* completeDownloadMenu_;

  // This is shown instead of progressView_ for dangerous downloads.
  IBOutlet NSView* dangerousDownloadView_;
  IBOutlet NSTextField* dangerousDownloadLabel_;
  IBOutlet NSButton* dangerousDownloadConfirmButton_;

  // Needed to find out how much the tweaker changed sizes to update the
  // other views.
  IBOutlet GTMWidthBasedTweaker* buttonTweaker_;

  // Because the confirm text and button for dangerous downloads are determined
  // at runtime, an outlet to the localizer is needed to construct the layout
  // tweaker in awakeFromNib in order to adjust the UI after all strings are
  // determined.
  IBOutlet ChromeUILocalizer* localizer_;

  IBOutlet NSImageView* image_;

  scoped_ptr<DownloadItemMac> bridge_;
  scoped_ptr<DownloadShelfContextMenuMac> menuBridge_;

  // Weak pointer to the shelf that owns us.
  DownloadShelfController* shelf_;

  // The time at which this view was created.
  base::Time creationTime_;

  // The state of this item.
  enum DownoadItemState {
    kNormal,
    kDangerous
  } state_;
};

// Takes ownership of |downloadModel|.
- (id)initWithModel:(BaseDownloadItemModel*)downloadModel
              shelf:(DownloadShelfController*)shelf;

// Updates the UI and menu state from |downloadModel|.
- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel;

// Remove ourself from the download UI.
- (void)remove;

// Update item's visibility depending on if the item is still completely
// contained in its parent.
- (void)updateVisibility:(id)sender;

// Called after a download is opened.
- (void)downloadWasOpened;

// Asynchronous icon loading callback.
- (void)setIcon:(NSImage*)icon;

// Download item button clicked
- (IBAction)handleButtonClick:(id)sender;

// Returns the size this item wants to have.
- (NSSize)preferredSize;

// Returns the DownloadItem model object belonging to this item.
- (DownloadItem*)download;

// Updates the tooltip with the download's path.
- (void)updateToolTip;

// Handling of dangerous downloads
- (void)clearDangerousMode;
- (BOOL)isDangerousMode;
- (IBAction)saveDownload:(id)sender;
- (IBAction)discardDownload:(id)sender;

// Context menu handlers.
- (IBAction)handleOpen:(id)sender;
- (IBAction)handleAlwaysOpen:(id)sender;
- (IBAction)handleReveal:(id)sender;
- (IBAction)handleCancel:(id)sender;
- (IBAction)handleTogglePause:(id)sender;

@end
