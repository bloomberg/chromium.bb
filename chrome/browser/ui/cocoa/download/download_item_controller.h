// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

@class ChromeUILocalizer;
@class DownloadItemCell;
@class DownloadItemButton;
class DownloadItemMac;
class DownloadItemModel;
class DownloadShelfContextMenuMac;
@class DownloadShelfController;
@class GTMWidthBasedTweaker;

namespace content {
class DownloadItem;
class PageNavigator;
}

namespace extensions {
class ExperienceSamplingEvent;
}

namespace gfx {
class FontList;
}

namespace ui {
class MenuModel;
}

// A controller class that manages one download item.
//
// The view hierarchy is as follows:
//
// DownloadItemController
//  |
//  |  A container that is showing one of its child views (progressView_ or
//  |  dangerousDownloadView_) depending on whether the download is safe or not.
//  |
//  +-- progressView_ (instance of DownloadItemButton)
//  |   |
//  |   +-- cell_ (instance of DownloadItemCell)
//  |
//  +-- dangerousDownloadView_
//      |
//      |  Contains the dangerous download warning. Dependong on whether the
//      |  download is dangerous (e.g. dangerous due to type of file), or
//      |  malicious (e.g. downloaded from a known malware site) only one of
//      |  dangerousButtonTweaker_ or maliciousButtonTweaker_ will be visible at
//      |  one time.
//      |
//      +-- dangerousDownloadLabel_
//      |
//      +-- image_
//      |
//      +-- dangerousButtonTweaker_
//      |
//      |  Contains the 'Discard'/'Save' buttons for a dangerous download. This
//      |  is a GTMWidthBasedTweaker that adjusts the width based on the text of
//      |  buttons.
//      |
//      +-- maliciousButtonTweaker_
//
//         Contains the 'Discard' button and the drop down context menu button.
//         This is a GTMWidthBasedTweaker that adjusts the width based on the
//         text of the 'Discard' button.
//
@interface DownloadItemController : NSViewController {
 @private
  IBOutlet DownloadItemButton* progressView_;
  IBOutlet DownloadItemCell* cell_;

  // This is shown instead of progressView_ for dangerous downloads.
  IBOutlet NSView* dangerousDownloadView_;
  IBOutlet NSTextField* dangerousDownloadLabel_;
  IBOutlet NSButton* dangerousDownloadConfirmButton_;

  // Needed to find out how much the tweakers changed sizes to update the other
  // views.
  IBOutlet GTMWidthBasedTweaker* dangerousButtonTweaker_;
  IBOutlet GTMWidthBasedTweaker* maliciousButtonTweaker_;

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

  // Default font list to use for text metrics.
  scoped_ptr<gfx::FontList> font_list_;

  // The state of this item.
  enum DownloadItemState {
    kNormal,
    kDangerous
  } state_;

  // ExperienceSampling: This tracks dangerous/malicious downloads warning UI
  // and the user's decisions about it.
  scoped_ptr<extensions::ExperienceSamplingEvent> sampling_event_;
};

// Initialize controller for |downloadItem|.
- (id)initWithDownload:(content::DownloadItem*)downloadItem
                 shelf:(DownloadShelfController*)shelf
             navigator:(content::PageNavigator*)navigator;

// Updates the UI and menu state from |downloadModel|.
- (void)setStateFromDownload:(DownloadItemModel*)downloadModel;

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
- (content::DownloadItem*)download;

// Returns the MenuModel for the download item context menu. The returned
// MenuModel is owned by the DownloadItemController and will be valid until the
// DownloadItemController is destroyed.
- (ui::MenuModel*)contextMenuModel;

// Updates the tooltip with the download's path.
- (void)updateToolTip;

// Handling of dangerous downloads
- (void)clearDangerousMode;
- (BOOL)isDangerousMode;
- (IBAction)saveDownload:(id)sender;
- (IBAction)discardDownload:(id)sender;
- (IBAction)dismissMaliciousDownload:(id)sender;
- (IBAction)showContextMenu:(id)sender;

@end
