// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class BaseDownloadItemModel;
class Browser;
@class BrowserWindowController;
@class DownloadItemController;
class DownloadShelf;
@class DownloadShelfView;

// A controller class that manages the download shelf for one window.

@interface DownloadShelfController : NSViewController {
 @private
  IBOutlet NSTextView* showAllDownloadsLink_;

  // Currently these two are always the same, but they mean slightly different
  // things. |contentAreaHasOffset_| is an implementation detail of the download
  // shelf visibility.
  BOOL contentAreaHasOffset_;
  BOOL barIsVisible_;

  scoped_ptr<DownloadShelf> bridge_;
  NSView* contentArea_;
  float shelfHeight_;

  // The download items we have added to our shelf.
  scoped_nsobject<NSMutableArray> downloadItemControllers_;
};

- (id)initWithBrowser:(Browser*)browser contentArea:(NSView*)content;

- (DownloadShelf*)bridge;
- (BOOL)isVisible;

- (IBAction)show:(id)sender;

// Run when the user clicks the close button on the right side of the shelf.
- (IBAction)hide:(id)sender;

- (void)addDownloadItem:(BaseDownloadItemModel*)model;

// Resizes the download shelf based on the state of the content area.
- (void)resizeDownloadShelf;

// Remove a download, possibly via clearing browser data.
- (void)remove:(DownloadItemController*)download;

// Notification that we are closing and should release our downloads.
- (void)exiting;

// Return the height of the download shelf.
- (float)height;

@end
