// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <map>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/bookmark_bar_bridge.h"
#include "webkit/glue/window_open_disposition.h"

@class BookmarkBarStateController;
class BookmarkModel;
class BookmarkNode;
@class BookmarkBarView;
class GURL;
class Profile;
class PrefService;
@protocol ViewResizer;

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

  // Our initial view width, which is applied in awakeFromNib.
  float initialWidth_;

  // BookmarkNodes have a 64bit id.  NSMenuItems have a 32bit tag used
  // to represent the bookmark node they refer to.  This map provides
  // a mapping from one to the other, so we can properly identify the
  // node from the item.  When adding items in, we start with seedId_.
  int32 seedId_;
  std::map<int32,int64> menuTagMap_;

  // Our bookmark buttons, ordered from L-->R.
  scoped_nsobject<NSMutableArray> buttons_;

  // If the bar is disabled, we hide it and ignore show/hide commands.
  // Set when using fullscreen mode.
  BOOL barIsEnabled_;

  // Set to YES when the user elects to always show the bookmark bar.
  BOOL barShouldBeShown_;

  // Bridge from Chrome-style C++ notifications (e.g. derived from
  // BookmarkModelObserver)
  scoped_ptr<BookmarkBarBridge> bridge_;

  // Delegate that can resize us.
  id<ViewResizer> resizeDelegate_;  // weak

  // Delegate that can open URLs for us.
  id<BookmarkURLOpener> urlDelegate_;  // weak

  IBOutlet NSView* buttonView_;
  IBOutlet NSButton* offTheSideButton_;
  IBOutlet NSMenu* buttonContextMenu_;
}

// Initializes the bookmark bar controller with the given browser
// profile and delegates.
- (id)initWithProfile:(Profile*)profile
         initialWidth:(float)initialWidth
       resizeDelegate:(id<ViewResizer>)resizeDelegate
          urlDelegate:(id<BookmarkURLOpener>)urlDelegate;

// Tell the bar to show itself if needed (e.g. if the kShowBookmarkBar
// is set).  Called once after the controller is first created.
- (void)showIfNeeded;

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
- (IBAction)openFolderMenuFromButton:(id)sender;
- (IBAction)openOffTheSideMenuFromButton:(id)sender;
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
@interface BookmarkBarController(InternalOrTestingAPI)
// Set the delegate for a unit test.
- (void)setUrlDelegate:(id<BookmarkURLOpener>)urlDelegate;
- (void)clearBookmarkBar;
- (NSView*)buttonView;
- (NSArray*)buttons;
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell xOffset:(int*)xOffset;
- (void)checkForBookmarkButtonGrowth:(NSButton*)button;
- (void)frameDidChange;
- (BOOL)offTheSideButtonIsHidden;
- (NSMenu *)menuForFolderNode:(const BookmarkNode*)node;
- (int64)nodeIdFromMenuTag:(int32)tag;
- (int32)menuTagFromNodeId:(int64)menuid;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
