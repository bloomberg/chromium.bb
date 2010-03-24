// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <map>

#import "base/chrome_application_mac.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_state.h"
#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#include "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"
#include "webkit/glue/window_open_disposition.h"

@class BookmarkBarController;
@class BookmarkBarFolderController;
@class BookmarkBarView;
@class BookmarkButton;
@class BookmarkFolderTarget;
class BookmarkModel;
@class BookmarkMenu;
class BookmarkNode;
class Browser;
class GURL;
@class MenuButton;
class PrefService;
class Profile;
class TabContents;
@class ToolbarController;
@protocol ViewResizer;

namespace bookmarks {

// Magic numbers from Cole
// TODO(jrg): create an objc-friendly version of bookmark_bar_constants.h?

// Used as a maximum width for buttons on the bar.
const CGFloat kDefaultBookmarkWidth = 150.0;

// TODO(jrg): http://crbug.com/36276 to get final sizes.
// Used as a min/max width for buttons on menus (not on the bar).
const CGFloat kBookmarkMenuButtonMinimumWidth = kDefaultBookmarkWidth;
const CGFloat kBookmarkMenuButtonMaximumWidth = 485.0;

const CGFloat kBookmarkVerticalPadding = 2.0;
const CGFloat kBookmarkHorizontalPadding = 1.0;
// (end magic numbers from Cole)

// Delay before opening a subfolder (and closing the previous one)
// when hovering over a folder button.
const NSTimeInterval kHoverOpenDelay = 0.3;

// Delay on hover before a submenu opens when dragging.
// Experimentally a drag hover open delay needs to be bigger than a
// normal (non-drag) menu hover open such as used in the bookmark folder.
//  TODO(jrg): confirm feel of this constant with ui-team.
//  http://crbug.com/36276
const NSTimeInterval kDragHoverOpenDelay = 0.7;

// Notes on use of kDragHoverCloseDelay in
// -[BookmarkBarFolderController draggingEntered:].
//
// We have an implicit delay on stop-hover-open before a submenu
// closes.  This cannot be zero since it's nice to move the mouse in a
// direct line from "current position" to "position of item in
// submenu".  However, by doing so, it's possible to overlap a
// different button on the current menu.  Example:
//
//  Folder1
//  Folder2  ---> Sub1
//  Folder3       Sub2
//                Sub3
//
// If you hover over the F in Folder2 to open the sub, and then want to
// select Sub3, a direct line movement of the mouse may cross over
// Folder3.  Without this delay, that'll cause Sub to be closed before
// you get there, since a "hover over" of Folder3 gets activated.
// It's subtle but without the delay it feels broken.
//
// This is only really a problem with vertical menu --> vertical menu
// movement; the bookmark bar (horizontal menu, sort of) seems fine,
// perhaps because mouse move direction is purely vertical so there is
// no opportunity for overlap.
const NSTimeInterval kDragHoverCloseDelay = 0.4;

}  // namespace bookmarks

// The interface for the bookmark bar controller's delegate. Currently, the
// delegate is the BWC and is responsible for ensuring that the toolbar is
// displayed correctly (as specified by |-getDesiredToolbarHeightCompression|
// and |-toolbarDividerOpacity|) at the beginning and at the end of an animation
// (or after a state change).
@protocol BookmarkBarControllerDelegate

// Sent when the state has changed (after any animation), but before the final
// display update.
- (void)bookmarkBar:(BookmarkBarController*)controller
 didChangeFromState:(bookmarks::VisualState)oldState
            toState:(bookmarks::VisualState)newState;

// Sent before the animation begins.
- (void)bookmarkBar:(BookmarkBarController*)controller
willAnimateFromState:(bookmarks::VisualState)oldState
            toState:(bookmarks::VisualState)newState;

@end

// A controller for the bookmark bar in the browser window. Handles showing
// and hiding based on the preference in the given profile.
@interface BookmarkBarController :
    NSViewController<BookmarkBarState,
                     BookmarkBarToolbarViewController,
                     BookmarkButtonDelegate,
                     BookmarkButtonControllerProtocol,
                     CrApplicationEventHookProtocol> {
 @private
  // The visual state of the bookmark bar. If an animation is running, this is
  // set to the "destination" and |lastVisualState_| is set to the "original"
  // state. This is set to |kInvalidState| on initialization (when the
  // appropriate state is not yet known).
  bookmarks::VisualState visualState_;

  // The "original" state of the bookmark bar if an animation is running,
  // otherwise it should be |kInvalidState|.
  bookmarks::VisualState lastVisualState_;

  Browser* browser_;              // weak; owned by its window
  BookmarkModel* bookmarkModel_;  // weak; part of the profile owned by the
                                  // top-level Browser object.

  // Our initial view width, which is applied in awakeFromNib.
  CGFloat initialWidth_;

  // BookmarkNodes have a 64bit id.  NSMenuItems have a 32bit tag used
  // to represent the bookmark node they refer to.  This map provides
  // a mapping from one to the other, so we can properly identify the
  // node from the item.  When adding items in, we start with seedId_.
  int32 seedId_;
  std::map<int32,int64> menuTagMap_;

  // Our bookmark buttons, ordered from L-->R.
  scoped_nsobject<NSMutableArray> buttons_;

  // The folder image so we can use one copy for all buttons
  scoped_nsobject<NSImage> folderImage_;

  // The default image, so we can use one copy for all buttons.
  scoped_nsobject<NSImage> defaultImage_;

  // If the bar is disabled, we hide it and ignore show/hide commands.
  // Set when using fullscreen mode.
  BOOL barIsEnabled_;

  // Bridge from Chrome-style C++ notifications (e.g. derived from
  // BookmarkModelObserver)
  scoped_ptr<BookmarkBarBridge> bridge_;

  // Delegate that is informed about state changes in the bookmark bar.
  id<BookmarkBarControllerDelegate> delegate_;  // weak

  // Delegate that can resize us.
  id<ViewResizer> resizeDelegate_;  // weak

  // Logic for dealing with a click on a bookmark folder button.
  scoped_nsobject<BookmarkFolderTarget> folderTarget_;

  // A controller for a pop-up bookmark folder window (custom menu).
  // This is not a scoped_nsobject because it owns itself (when its
  // window closes the controller gets autoreleased).
  BookmarkBarFolderController* folderController_;

  BOOL watchingForClickOutside_;  // Are watching for a "click outside"?

  IBOutlet BookmarkBarView* buttonView_;
  IBOutlet MenuButton* offTheSideButton_;  // aka the chevron
  IBOutlet NSMenu* buttonContextMenu_;

  // "Other bookmarks" button on the right side.
  scoped_nsobject<NSButton> otherBookmarksButton_;

  // We have a special menu for folder buttons.  This starts as a copy
  // of the bar menu.
  scoped_nsobject<BookmarkMenu> buttonFolderContextMenu_;

  // When doing a drag, this is folder button "hovered over" which we
  // may want to open after a short delay.  There are cases where a
  // mouse-enter can open a folder (e.g. if the menus are "active")
  // but that doesn't use this variable or need a delay so "hover" is
  // the wrong term.
  scoped_nsobject<BookmarkButton> hoverButton_;

  // We save the view width when we add bookmark buttons.  This lets
  // us avoid a rebuild until we've grown the window bigger than our
  // initial build.
  CGFloat savedFrameWidth_;

  // The number of buttons we display in the bookmark bar.  This does
  // not include the "off the side" chevron or the "Other Bookmarks"
  // button.  We use this number to determine if we need to display
  // the chevron, and to know what to place in the chevron's menu.
  // Since we create everything before doing layout we can't be sure
  // that all bookmark buttons we create will be visible.  Thus,
  // [buttons_ count] isn't a definitive check.
  int bookmarkBarDisplayedButtons_;

  // We only build the off-the-side menu on demand.
  // We flag a need to rebuild with this variable.
  BOOL needToRebuildOffTheSideMenu_;
}

@property(readonly, nonatomic) bookmarks::VisualState visualState;
@property(readonly, nonatomic) bookmarks::VisualState lastVisualState;
@property(assign, nonatomic) id<BookmarkBarControllerDelegate> delegate;

// Initializes the bookmark bar controller with the given browser
// profile and delegates.
- (id)initWithBrowser:(Browser*)browser
         initialWidth:(CGFloat)initialWidth
             delegate:(id<BookmarkBarControllerDelegate>)delegate
       resizeDelegate:(id<ViewResizer>)resizeDelegate;

// Updates the bookmark bar (from its current, possibly in-transition) state to
// the one appropriate for the new conditions.
- (void)updateAndShowNormalBar:(BOOL)showNormalBar
               showDetachedBar:(BOOL)showDetachedBar
                 withAnimation:(BOOL)animate;

// Update the visible state of the bookmark bar.
- (void)updateVisibility;

// Turn on or off the bookmark bar and prevent or reallow its appearance. On
// disable, toggle off if shown. On enable, show only if needed. App and popup
// windows do not show a bookmark bar.
- (void)setBookmarkBarEnabled:(BOOL)enabled;

// Returns the amount by which the toolbar above should be compressed.
- (CGFloat)getDesiredToolbarHeightCompression;

// Gets the appropriate opacity for the toolbar's divider; 0 means that it
// shouldn't be shown.
- (CGFloat)toolbarDividerOpacity;

// Returns true if at least one bookmark was added.
- (BOOL)addURLs:(NSArray*)urls withTitles:(NSArray*)titles at:(NSPoint)point;

// Updates the sizes and positions of the subviews.
// TODO(viettrungluu): I'm not convinced this should be public, but I currently
// need it for animations. Try not to propagate its use.
- (void)layoutSubviews;

// Called by our view when it is moved to a window.
- (void)viewDidMoveToWindow;

// Import bookmarks from another browser.
- (IBAction)importBookmarks:(id)sender;

// Actions for manipulating bookmarks.
// Open a normal bookmark or folder from a button, ...
- (IBAction)openBookmark:(id)sender;
- (IBAction)openBookmarkFolderFromButton:(id)sender;
// From a context menu over the button, ...
- (IBAction)openBookmarkInNewForegroundTab:(id)sender;
- (IBAction)openBookmarkInNewWindow:(id)sender;
- (IBAction)openBookmarkInIncognitoWindow:(id)sender;
- (IBAction)editBookmark:(id)sender;
- (IBAction)copyBookmark:(id)sender;
- (IBAction)deleteBookmark:(id)sender;
// From a context menu over the bar, ...
- (IBAction)openAllBookmarks:(id)sender;
- (IBAction)openAllBookmarksNewWindow:(id)sender;
- (IBAction)openAllBookmarksIncognitoWindow:(id)sender;
// Or from a context menu over either the bar or a button.
- (IBAction)addPage:(id)sender;
- (IBAction)addFolder:(id)sender;

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
- (void)openURL:(GURL)url disposition:(WindowOpenDisposition)disposition;
- (NSCell*)cellForBookmarkNode:(const BookmarkNode*)node;
- (void)clearBookmarkBar;
- (BookmarkBarView*)buttonView;
- (NSArray*)buttons;
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell xOffset:(int*)xOffset;
- (void)checkForBookmarkButtonGrowth:(NSButton*)button;
- (void)frameDidChange;
- (BOOL)offTheSideButtonIsHidden;
- (NSMenu *)menuForFolderNode:(const BookmarkNode*)node;
- (int64)nodeIdFromMenuTag:(int32)tag;
- (int32)menuTagFromNodeId:(int64)menuid;
- (void)buildOffTheSideMenuIfNeeded;
- (NSMenu*)offTheSideMenu;
- (NSButton*)offTheSideButton;
- (NSButton*)otherBookmarksButton;
- (const BookmarkNode*)nodeFromMenuItem:(id)sender;
- (void)updateTheme:(ThemeProvider*)themeProvider;
- (BookmarkBarFolderController*)folderController;
- (BookmarkButton*)buttonForDroppingOnAtPoint:(NSPoint)point;
- (BOOL)isEventAClickOutside:(NSEvent*)event;
- (id)folderTarget;
@end

// The (internal) |NSPasteboard| type string for bookmark button drags, used for
// dragging buttons around the bookmark bar. The data for this type is just a
// pointer to the |BookmarkButton| being dragged.
extern NSString* kBookmarkButtonDragType;

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_CONTROLLER_H_
