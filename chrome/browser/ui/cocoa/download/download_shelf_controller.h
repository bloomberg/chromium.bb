// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"

@class AnimatableView;
class BaseDownloadItemModel;
class Browser;
@class BrowserWindowController;
@class DownloadItemController;
class DownloadShelf;
@class DownloadShelfView;
@class HyperlinkButtonCell;
@class HoverButton;

// A controller class that manages the download shelf for one window. It is
// responsible for the behavior of the shelf itself (showing/hiding, handling
// the link, layout) as well as for managing the download items it contains.
//
// All the files in cocoa/downloads_* are related as follows:
//
// download_shelf_mac bridges calls from chromium's c++ world to the objc
// download_shelf_controller for the shelf (this file). The shelf's background
// is drawn by download_shelf_view. Every item in a shelf is controlled by a
// download_item_controller.
//
// download_item_mac bridges calls from chromium's c++ world to the objc
// download_item_controller, which is responsible for managing a single item
// on the shelf. The item controller loads its UI from a xib file, where the
// UI of an item itself is represented by a button that is drawn by
// download_item_cell.

@interface DownloadShelfController : NSViewController<NSTextViewDelegate> {
 @private
  IBOutlet HyperlinkButtonCell* showAllDownloadsCell_;

  IBOutlet NSImageView* image_;

  IBOutlet HoverButton* hoverCloseButton_;

  BOOL barIsVisible_;

  BOOL isFullscreen_;

  scoped_ptr<DownloadShelf> bridge_;

  // Height of the shelf when it's fully visible.
  CGFloat maxShelfHeight_;

  // Current height of the shelf. Changes while the shelf is animating in or
  // out.
  CGFloat currentShelfHeight_;

  // Used to autoclose the shelf when the mouse is moved off it.  Is non-nil
  // only when a subsequent mouseExited event can trigger autoclose or when a
  // subsequent mouseEntered event will cancel autoclose.  Is nil otherwise.
  scoped_nsobject<NSTrackingArea> trackingArea_;

  // The download items we have added to our shelf.
  scoped_nsobject<NSMutableArray> downloadItemControllers_;

  // The container that contains (and clamps) all the download items.
  IBOutlet NSView* itemContainerView_;

  // Delegate that handles resizing our view.
  id<ViewResizer> resizeDelegate_;
};

- (id)initWithBrowser:(Browser*)browser
       resizeDelegate:(id<ViewResizer>)resizeDelegate;

- (IBAction)showDownloadsTab:(id)sender;

// Returns our view cast as an AnimatableView.
- (AnimatableView*)animatableView;

- (DownloadShelf*)bridge;
- (BOOL)isVisible;

- (IBAction)show:(id)sender;

// Run when the user clicks the close button on the right side of the shelf.
- (IBAction)hide:(id)sender;

- (void)addDownloadItem:(BaseDownloadItemModel*)model;

// Remove a download, possibly via clearing browser data.
- (void)remove:(DownloadItemController*)download;

// Called by individual item controllers when their downloads are opened.
- (void)downloadWasOpened:(DownloadItemController*)download;

// Notification that we are closing and should release our downloads.
- (void)exiting;

// Return the height of the download shelf.
- (float)height;

// Re-layouts all download items based on their current state.
- (void)layoutItems;

@end
