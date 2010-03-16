// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmark_button.h"

@class BookmarkBarFolderView;

// A controller for the pop-up windows from bookmark folder buttons
// which look sort of like menus.
@interface BookmarkBarFolderController :
    NSWindowController<BookmarkButtonDelegate,
                       BookmarkButtonControllerProtocol> {
 @private
  // The button whose click opened us.
  scoped_nsobject<BookmarkButton> parentButton_;

  // Bookmark bar folder controller chains are torn down in two ways:
  // 1. Clicking "outside" the folder (see use of
  // CrApplicationEventHookProtocol in the bookmark bar controller).
  // 2. Engaging a different folder (via hover over or explicit click).
  //
  // In either case, the BookmarkButtonControllerProtocol method
  // closeAllBookmarkFolders gets called.  For bookmark bar folder
  // controllers, this is passed up the chain so we begin with a top
  // level "close".
  // When any bookmark folder window closes, it necessarily tells
  // subcontroller windows to close (down the chain), and autoreleases
  // the controller.  (Must autorelease since the controller can still
  // get delegate events such as windowDidClose).
  //
  // Bookmark bar folder controllers own their buttons.  When doing
  // drag and drop of a button from one sub-sub-folder to a different
  // sub-sub-folder, we need to make sure the button's pointers stay
  // valid until we've dropped (or cancelled).  Note that such a drag
  // causes the source sub-sub-folder (previous parent window) to go
  // away (windows close, controllers autoreleased) since you're
  // hovering over a different folder chain for dropping.  To keep
  // things valid (like the button's target, its delegate, the parent
  // cotroller that we have a pointer to below [below], etc), we heep
  // strong pointers to our owning controller, so the entire chain
  // stays owned.

  // Our parent controller.  This may be another
  // BookmarkBarFolderController (if we are a nested folder) or it may
  // be the BookmarkBarController (if not).
  // Strong to insure the object lives as long as we need it.
  scoped_nsobject<NSObject<BookmarkButtonControllerProtocol> >
      parentController_;

  // Our buttons.  We do not have buttons for nested folders.
  scoped_nsobject<NSMutableArray> buttons_;

  // The main view of this window (where the buttons go).
  IBOutlet BookmarkBarFolderView* mainView_;

  // Like normal menus, hovering over a folder button causes it to
  // open.  This variable is set when a hover is initiated (but has
  // not necessarily fired yet).
  scoped_nsobject<BookmarkButton> hoverButton_;

  // A controller for a pop-up bookmark folder window (custom menu).
  // We (self) are the parentController_ for our folderController_.
  // This is not a scoped_nsobject because it owns itself (when its
  // window closes the controller gets autoreleased).
  BookmarkBarFolderController* folderController_;

  // Has a draggingExited been called?  Only relevant for
  // performSelector:after:delay: calls that get triggered in the
  // middle of a drag.
  BOOL draggingExited_;

  // Implement basic menu scrolling through this tracking area.
  scoped_nsobject<NSTrackingArea> scrollTrackingArea_;

  // Timer to continue scrolling as needed.  We own the timer but
  // don't release it when done (we invalidate it).
  NSTimer* scrollTimer_;

  // Amount to scroll by on each timer fire.  Can be + or -.
  CGFloat verticalScrollDelta_;
}

- (id)initWithParentButton:(BookmarkButton*)button
      parentController:(NSObject<BookmarkButtonControllerProtocol>*)controller;

// Return the parent button that owns the bookmark folder we represent.
- (BookmarkButton*)parentButton;

@end


@interface BookmarkBarFolderController(TestingAPI)
- (NSView*)mainView;
- (NSPoint)windowTopLeft;
- (NSArray*)buttons;
- (BookmarkBarFolderController*)folderController;
@end

