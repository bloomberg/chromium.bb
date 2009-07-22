// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/bookmark_bar_bridge.h"
#include "webkit/glue/window_open_disposition.h"

@class BookmarkBarStateController;
class BookmarkModel;
class BookmarkNode;
@class BookmarkBarView;
class Profile;
class PrefService;

// The interface for an object which can open URLs for a bookmark.
@protocol BookmarkURLOpener
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition;
@end


// A controller for the bookmark bar in the browser window. Handles showing
// and hiding based on the preference in the given profile.
@interface BookmarkBarController : NSViewController {
 @private
  Profile* profile_;              // weak
  BookmarkModel* bookmarkModel_;  // weak; part of the profile owned by the
                                  // top-level Browser object.

  // Currently these two are always the same when not in fullscreen
  // mode, but they mean slightly different things.
  // contentAreaHasOffset_ is an implementation detail of bookmark bar
  // show state.
  BOOL contentViewHasOffset_;
  BOOL barShouldBeShown_;

  // Our bookmark buttons, ordered from L-->R.
  scoped_nsobject<NSMutableArray> buttons_;

  // If the bar is disabled, we hide it and ignore show/hide commands.
  // Set when using fullscreen mode.
  BOOL barIsEnabled_;

  NSView* parentView_;      // weak; our parent view
  NSView* webContentView_;  // weak; where the web goes
  NSView* infoBarsView_;    // weak; where the infobars go

  // Bridge from Chrome-style C++ notifications (e.g. derived from
  // BookmarkModelObserver)
  scoped_ptr<BookmarkBarBridge> bridge_;

  // Delegate which can open URLs for us.
  id<BookmarkURLOpener> delegate_;  // weak

  IBOutlet NSMenu* buttonContextMenu_;
}

// Initializes the bookmark bar controller with the given browser
// profile, parent view (the toolbar), web content view, and delegate.
// |delegate| is used for opening URLs.
// TODO(rohitrao, jrg): The bookmark bar shouldn't know about the
// infoBarsView or the webContentView.
- (id)initWithProfile:(Profile*)profile
           parentView:(NSView*)parentView
       webContentView:(NSView*)webContentView
         infoBarsView:(NSView*)infoBarsView
             delegate:(id<BookmarkURLOpener>)delegate;

// Returns whether or not the bookmark bar is visible.
- (BOOL)isBookmarkBarVisible;

// Toggle the state of the bookmark bar.
- (void)toggleBookmarkBar;

// Turn on or off the bookmark bar and prevent or reallow its
// appearance.  On disable, toggle off if shown.  On enable, show only
// if needed.  For fullscreen mode.
- (void)setBookmarkBarEnabled:(BOOL)enabled;

// Actions for manipulating bookmarks.
// From a button, ...
- (IBAction)openBookmark:(id)sender;
// From a context menu over the button, ...
- (IBAction)openBookmarkInNewForegroundTab:(id)sender;
- (IBAction)openBookmarkInNewWindow:(id)sender;
- (IBAction)openBookmarkInIncognitoWindow:(id)sender;
- (IBAction)editBookmark:(id)sender;
- (IBAction)deleteBookmark:(id)sender;
// From a context menu over the bar, ...
- (IBAction)openAllBookmarks:(id)sender;
// Or from a context menu over either the bar or a button.
- (IBAction)addPage:(id)sender;
- (IBAction)addOrRenameFolder:(id)sender;


@end

// Redirects from BookmarkBarBridge, the C++ object which glues us to
// the rest of Chromium.  Internal to BookmarkBarController.
@interface BookmarkBarController(BridgeRedirect)
- (void)loaded:(BookmarkModel*)model;
- (void)beingDeleted:(BookmarkModel*)model;
- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex;
- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)oldParent index:(int)index;
- (void)nodeRemoved:(BookmarkModel*)model
             parent:(const BookmarkNode*)oldParent index:(int)index;
- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node;
- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node;
- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node;
@end


// These APIs should only be used by unit tests (or used internally).
@interface BookmarkBarController(TestingAPI)
// Set the delegate for a unit test.
- (void)setDelegate:(id<BookmarkURLOpener>)delegate;
- (void)clearBookmarkBar;
- (NSArray*)buttons;
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell xOffset:(int*)xOffset;
- (void)checkForBookmarkButtonGrowth:(NSButton*)button;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
