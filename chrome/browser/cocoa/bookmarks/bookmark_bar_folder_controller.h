// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_button.h"

@class BookmarkBarController;
@class BookmarkBarFolderView;
@class BookmarkFolderTarget;
@class BookmarkBarFolderHoverState;

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

  // Our parent controller, if we are a nested folder, otherwise nil.
  // Strong to insure the object lives as long as we need it.
  scoped_nsobject<BookmarkBarFolderController> parentController_;

  // The main bar controller from whence we or a parent sprang.
  BookmarkBarController* barController_;  // WEAK: It owns us.

  // Our buttons.  We do not have buttons for nested folders.
  scoped_nsobject<NSMutableArray> buttons_;

  // The scroll view that contains our main button view (below).
  IBOutlet NSScrollView* scrollView_;

  // Are we scrollable?  If no, the full contents of the folder are
  // always visible.
  BOOL scrollable_;

  BOOL scrollUpArrowShown_;
  BOOL scrollDownArrowShown_;

  // YES if subfolders should grow to the right (the default).
  // Direction switches if we'd grow off the screen.
  BOOL subFolderGrowthToRight_;

  // The main view of this window (where the buttons go).
  IBOutlet BookmarkBarFolderView* mainView_;

  // Weak; we keep track to work around a
  // setShowsBorderOnlyWhileMouseInside bug.
  BookmarkButton* buttonThatMouseIsIn_;

  // The context menu for a bookmark button which represents an URL.
  IBOutlet NSMenu* buttonMenu_;

  // The context menu for a bookmark button which represents a folder.
  IBOutlet NSMenu* folderMenu_;

  // We model hover state as a state machine with specific allowable
  // transitions.  |hoverState_| is the state of this machine at any
  // given time.
  scoped_nsobject<BookmarkBarFolderHoverState> hoverState_;

  // Logic for dealing with a click on a bookmark folder button.
  scoped_nsobject<BookmarkFolderTarget> folderTarget_;

  // A controller for a pop-up bookmark folder window (custom menu).
  // We (self) are the parentController_ for our folderController_.
  // This is not a scoped_nsobject because it owns itself (when its
  // window closes the controller gets autoreleased).
  BookmarkBarFolderController* folderController_;

  // Implement basic menu scrolling through this tracking area.
  scoped_nsobject<NSTrackingArea> scrollTrackingArea_;

  // Timer to continue scrolling as needed.  We own the timer but
  // don't release it when done (we invalidate it).
  NSTimer* scrollTimer_;

  // Amount to scroll by on each timer fire.  Can be + or -.
  CGFloat verticalScrollDelta_;

  // We need to know the size of the vertical scrolling arrows so we
  // can obscure/unobscure them.
  CGFloat verticalScrollArrowHeight_;

  // Set to YES to prevent any node animations. Useful for unit testing so that
  // incomplete animations do not cause valgrind complaints.
  BOOL ignoreAnimations_;
}

// Designated initializer.
- (id)initWithParentButton:(BookmarkButton*)button
          parentController:(BookmarkBarFolderController*)parentController
             barController:(BookmarkBarController*)barController;

// Return the parent button that owns the bookmark folder we represent.
- (BookmarkButton*)parentButton;

// Offset our folder menu window. This is usually needed in response to a
// parent folder menu window or the bookmark bar changing position due to
// the dragging of a bookmark node from the parent into this folder menu.
- (void)offsetFolderMenuWindow:(NSSize)offset;

// Re-layout the window menu in case some buttons were added or removed,
// specifically as a result of the bookmark bar changing configuration
// and altering the contents of the off-the-side folder.
- (void)reconfigureMenu;

// Actions from a context menu over a button or folder.
- (IBAction)cutBookmark:(id)sender;
- (IBAction)copyBookmark:(id)sender;
- (IBAction)pasteBookmark:(id)sender;
- (IBAction)deleteBookmark:(id)sender;

// Passed up by a child view to tell us of a desire to scroll.
- (void)scrollWheel:(NSEvent *)theEvent;

// Forwarded to the associated BookmarkBarController.
- (IBAction)addFolder:(id)sender;
- (IBAction)addPage:(id)sender;
- (IBAction)editBookmark:(id)sender;
- (IBAction)openBookmark:(id)sender;
- (IBAction)openAllBookmarks:(id)sender;
- (IBAction)openAllBookmarksIncognitoWindow:(id)sender;
- (IBAction)openAllBookmarksNewWindow:(id)sender;
- (IBAction)openBookmarkInIncognitoWindow:(id)sender;
- (IBAction)openBookmarkInNewForegroundTab:(id)sender;
- (IBAction)openBookmarkInNewWindow:(id)sender;

@property (assign, nonatomic) BOOL subFolderGrowthToRight;

@end

@interface BookmarkBarFolderController(TestingAPI)
- (NSView*)mainView;
- (NSPoint)windowTopLeftForWidth:(int)windowWidth;
- (NSArray*)buttons;
- (BookmarkBarFolderController*)folderController;
- (id)folderTarget;
- (void)configureWindowLevel;
- (void)performOneScroll:(CGFloat)delta;
- (BookmarkButton*)buttonThatMouseIsIn;
// Set to YES in order to prevent animations.
- (void)setIgnoreAnimations:(BOOL)ignore;

// Return YES if we can scroll up or down.
- (BOOL)canScrollUp;
- (BOOL)canScrollDown;
// Return YES if the scrollable_ flag has been set.
- (BOOL)scrollable;

- (BookmarkButton*)buttonForDroppingOnAtPoint:(NSPoint)point;
@end
