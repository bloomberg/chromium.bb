// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/draggable_button.h"

@class BookmarkButton;
class BookmarkModel;
class BookmarkNode;
@class BrowserWindowController;
class ThemeProvider;

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

@end


// Protocol to be implemented by controllers that logically own
// bookmark buttons.  The controller may be either an NSViewController
// or NSWindowController.  The BookmarkButton doesn't use this
// protocol directly; it is used when BookmarkButton controllers talk
// to each other.
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
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info;
- (void)draggingExited:(id<NSDraggingInfo>)info;

// Perform the actual DnD of a bookmark button.

// |point| is in the base coordinate system of the destination window;
// |it comes from an id<NSDraggingInfo>. |copy| is YES if a copy is to be
// made and inserted into the new location while leaving the bookmark in
// the old location, otherwise move the bookmark by removing from its old
// location and inserting into the new location.
- (BOOL)dragButton:(BookmarkButton *)sourceButton
                to:(NSPoint)point
              copy:(BOOL)copy;

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
- (CGFloat)indicatorPosForDragOfButton:(BookmarkButton*)sourceButton
                               toPoint:(NSPoint)point;

// Return the parent window for all BookmarkBarFolderController windows.
- (NSWindow*)parentWindow;

// Return the theme provider associated with this browser window.
- (ThemeProvider*)themeProvider;

// Called just before a child folder puts itself on screen.
- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child;

// Called just before a child folder closes.
- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child;

@end  // @protocol BookmarkButtonControllerProtocol


// Class for bookmark bar buttons that can be drag sources.
@interface BookmarkButton : DraggableButton {
 @private
  NSObject<BookmarkButtonDelegate>* delegate_;  // weak like all delegates

  // Saved pointer to the BWC for the browser window that contains this button.
  // Used to lock and release bar visibility during a drag.  The pointer is
  // saved because the bookmark button is no longer a part of a window at the
  // end of a drag operation (or, in fact, can be dragged to a completely
  // different window), so there is no way to retrieve the same BWC object after
  // a drag.
  BrowserWindowController* visibilityDelegate_;  // weak
}

@property(assign, nonatomic) NSObject<BookmarkButtonDelegate>* delegate;

// Return the bookmark node associated with this button, or NULL.
- (const BookmarkNode*)bookmarkNode;

// Return YES if this is a folder button (the node has subnodes).
- (BOOL)isFolder;

// At this time we represent an empty folder (e.g. the string
// '(empty)') as a disabled button with no associated node.
//
// TODO(jrg): improve; things work but are slightly ugly since "empty"
// and "one disabled button" are not the same thing.
// http://crbug.com/35967
- (BOOL)isEmpty;

@end  // @interface BookmarkButton


@interface BookmarkButton(TestingAPI)
- (void)beginDrag:(NSEvent*)event;
@end

