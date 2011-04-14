// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>
#import "chrome/browser/ui/cocoa/draggable_button.h"
#include "webkit/glue/window_open_disposition.h"

@class BookmarkBarFolderController;
@class BookmarkButton;
struct BookmarkNodeData;
class BookmarkModel;
class BookmarkNode;
@class BrowserWindowController;

namespace ui {
class ThemeProvider;
}

// Protocol for a BookmarkButton's delegate, responsible for doing
// things on behalf of a bookmark button.
@protocol BookmarkButtonDelegate

// Fill the given pasteboard with appropriate data when the given button is
// dragged. Since the delegate has no way of providing pasteboard data later,
// all data must actually be put into the pasteboard and not merely promised.
- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button;

// Bookmark buttons pass mouseEntered: and mouseExited: events to
// their delegate.  This allows the delegate to decide (for example)
// which one, if any, should perform a hover-open.
- (void)mouseEnteredButton:(id)button event:(NSEvent*)event;
- (void)mouseExitedButton:(id)button event:(NSEvent*)event;

// Returns YES if a drag operation should lock the fullscreen overlay bar
// visibility before starting.  For example, dragging a bookmark button should
// not lock the overlay if the bookmark bar is currently showing in detached
// mode on the NTP.
- (BOOL)dragShouldLockBarVisibility;

// Returns the top-level window for this button.
- (NSWindow*)browserWindow;

// Returns YES if the bookmark button can be dragged to the trash, NO otherwise.
- (BOOL)canDragBookmarkButtonToTrash:(BookmarkButton*)button;

// This is called after the user has dropped the bookmark button on the trash.
// The delegate can use this event to delete the bookmark.
- (void)didDragBookmarkToTrash:(BookmarkButton*)button;

// This is called after the drag has finished, for any reason.
// We particularly need this so we can hide bookmark folder menus and stop
// doing that hover thing.
- (void)bookmarkDragDidEnd:(BookmarkButton*)button
                 operation:(NSDragOperation)operation;

@end


// Protocol to be implemented by controllers that logically own
// bookmark buttons.  The controller may be either an NSViewController
// or NSWindowController.  The BookmarkButton doesn't use this
// protocol directly; it is used when BookmarkButton controllers talk
// to each other.
//
// Other than the top level owner (the bookmark bar), all bookmark
// button controllers have a parent controller.
@protocol BookmarkButtonControllerProtocol

// Close all bookmark folders, walking up the ownership chain.
- (void)closeAllBookmarkFolders;

// Close just my bookmark folder.
- (void)closeBookmarkFolder:(id)sender;

// Return the bookmark model for this controller.
- (BookmarkModel*)bookmarkModel;

// Perform drag enter/exit operations, such as hover-open and hover-close.
- (BOOL)draggingAllowed:(id<NSDraggingInfo>)info;
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info;
- (void)draggingExited:(id<NSDraggingInfo>)info;

// Returns YES if a drag operation should lock the fullscreen overlay bar
// visibility before starting.  For example, dragging a bookmark button should
// not lock the overlay if the bookmark bar is currently showing in detached
// mode on the NTP.
- (BOOL)dragShouldLockBarVisibility;

// Perform the actual DnD of a bookmark or bookmark button.

// |point| is in the base coordinate system of the destination window;
// |it comes from an id<NSDraggingInfo>. |copy| is YES if a copy is to be
// made and inserted into the new location while leaving the bookmark in
// the old location, otherwise move the bookmark by removing from its old
// location and inserting into the new location.
- (BOOL)dragButton:(BookmarkButton*)sourceButton
                to:(NSPoint)point
              copy:(BOOL)copy;

// Determine if the pasteboard from |info| has dragging data containing
// bookmark(s) and perform the drag and return YES, otherwise return NO.
- (BOOL)dragBookmarkData:(id<NSDraggingInfo>)info;

// Determine if the drag pasteboard has any drag data of type
// kBookmarkDictionaryListPboardType and, if so, return those elements
// otherwise return an empty vector.
- (std::vector<const BookmarkNode*>)retrieveBookmarkNodeData;

// Return YES if we should show the drop indicator, else NO.  In some
// cases (e.g. hover open) we don't want to show the drop indicator.
// |point| is in the base coordinate system of the destination window;
// |it comes from an id<NSDraggingInfo>.
- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)point;

// The x or y coordinate of (the middle of) the indicator to draw for
// a drag of the source button to the given point (given in window
// coordinates).
// |point| is in the base coordinate system of the destination window;
// |it comes from an id<NSDraggingInfo>.
// TODO(viettrungluu,jrg): instead of this, make buttons move around.
// http://crbug.com/35968
- (CGFloat)indicatorPosForDragToPoint:(NSPoint)point;

// Used to tell the controller that room should be made for a drop.
- (void)setDropInsertionPos:(CGFloat)where;

// Used to tell the controller to stop making room for a drop.
- (void)clearDropInsertionPos;

// Return the theme provider associated with this browser window.
- (ui::ThemeProvider*)themeProvider;

// Called just before a child folder puts itself on screen.
- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child;

// Called just before a child folder closes.
- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child;

// Return a controller's folder controller for a subfolder, or nil.
- (BookmarkBarFolderController*)folderController;

// Add a new folder controller as triggered by the given folder button.
// If there is a current folder controller, close it.
- (void)addNewFolderControllerWithParentButton:(BookmarkButton*)parentButton;

// Open all of the nodes for the given node with disposition.
- (void)openAll:(const BookmarkNode*)node
    disposition:(WindowOpenDisposition)disposition;

// There are several operations which may affect the contents of a bookmark
// button controller after it has been created, primary of which are
// cut/paste/delete and drag/drop. Such changes may involve coordinating
// the bookmark button contents of two controllers (such as when a bookmark is
// dragged from one folder to another).  The bookmark bar controller
// coordinates in response to notifications propagated by the bookmark model
// through BookmarkBarBridge calls. The following three functions are
// implemented by the controllers and are dispatched by the bookmark bar
// controller in response to notifications coming in from the BookmarkBarBridge.

// Add a button for the given node to the bar or folder menu. This is safe
// to call when a folder menu window is open as that window will be updated.
// And index of -1 means to append to the end (bottom).
- (void)addButtonForNode:(const BookmarkNode*)node
                 atIndex:(NSInteger)buttonIndex;

// Given a list or |urls| and |titles|, create new bookmark nodes and add
// them to the bookmark model such that they will be 1) added to the folder
// represented by the button at |point| if it is a folder, or 2) inserted
// into the parent of the non-folder bookmark at |point| in front of that
// button. Returns YES if at least one bookmark was added.
// TODO(mrossetti): Change function to use a pair-like structure for
// URLs and titles. http://crbug.com/44411
- (BOOL)addURLs:(NSArray*)urls withTitles:(NSArray*)titles at:(NSPoint)point;

// Move a button from one place in the menu to another. This is safe
// to call when a folder menu window is open as that window will be updated.
- (void)moveButtonFromIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex;

// Remove the bookmark button at the given index. Show the poof animation
// if |animate:| is YES.  It may be obvious, but this is safe
// to call when a folder menu window is open as that window will be updated.
- (void)removeButton:(NSInteger)buttonIndex animate:(BOOL)poof;

// Determine the controller containing the button representing |node|, if any.
- (id<BookmarkButtonControllerProtocol>)controllerForNode:
      (const BookmarkNode*)node;

@end  // @protocol BookmarkButtonControllerProtocol


// Class for bookmark bar buttons that can be drag sources.
@interface BookmarkButton : DraggableButton {
 @private
  IBOutlet NSObject<BookmarkButtonDelegate>* delegate_;  // Weak.

  // Saved pointer to the BWC for the browser window that contains this button.
  // Used to lock and release bar visibility during a drag.  The pointer is
  // saved because the bookmark button is no longer a part of a window at the
  // end of a drag operation (or, in fact, can be dragged to a completely
  // different window), so there is no way to retrieve the same BWC object after
  // a drag.
  BrowserWindowController* visibilityDelegate_;  // weak

  NSPoint dragMouseOffset_;
  NSPoint dragEndScreenLocation_;
  BOOL dragPending_;
  BOOL acceptsTrackIn_;
  NSTrackingArea* area_;
}

@property(assign, nonatomic) NSObject<BookmarkButtonDelegate>* delegate;
@property(assign, nonatomic) BOOL acceptsTrackIn;

// Return the bookmark node associated with this button, or NULL.
- (const BookmarkNode*)bookmarkNode;

// Return YES if this is a folder button (the node has subnodes).
- (BOOL)isFolder;

- (void)mouseDragged:(NSEvent*)theEvent;

- (BOOL)acceptsTrackInFrom:(id)sender;

// At this time we represent an empty folder (e.g. the string
// '(empty)') as a disabled button with no associated node.
//
// TODO(jrg): improve; things work but are slightly ugly since "empty"
// and "one disabled button" are not the same thing.
// http://crbug.com/35967
- (BOOL)isEmpty;

// Turn on or off pulsing of a bookmark button.
// Triggered by the bookmark bubble.
- (void)setIsContinuousPulsing:(BOOL)flag;

// Return continuous pulse state.
- (BOOL)isContinuousPulsing;

// Return the location in screen coordinates where the remove animation should
// be displayed.
- (NSPoint)screenLocationForRemoveAnimation;

// The BookmarkButton which is currently being dragged, if any.
+ (BookmarkButton*)draggedButton;


@end  // @interface BookmarkButton


@interface BookmarkButton(TestingAPI)
- (void)beginDrag:(NSEvent*)event;
@end

namespace bookmark_button {

// Notifications for pulsing of bookmarks.
extern NSString* const kPulseBookmarkButtonNotification;

// Key for userInfo dict of a kPulseBookmarkButtonNotification.
// Value is a [NSValue valueWithPointer:]; pointer is a (const BookmarkNode*).
extern NSString* const kBookmarkKey;

// Key for userInfo dict of a kPulseBookmarkButtonNotification.
// Value is a [NSNumber numberWithBool:] to turn pulsing on or off.
extern NSString* const kBookmarkPulseFlagKey;

};
