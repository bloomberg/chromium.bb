// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "base/scoped_nsobject.h"

class BookmarkModel;
class BookmarkNode;
@class BookmarkBubbleController;

// Protocol for a BookmarkBubbleController's (BBC's) delegate.
@protocol BookmarkBubbleControllerDelegate

// The bubble asks the delegate to perform an edit when needed.
- (void)editBookmarkNode:(const BookmarkNode*)node;

// The bubble tells its delegate when it's done and can be deallocated.
- (void)doneWithBubbleController:(BookmarkBubbleController*)controller;

@end

// Controller for the bookmark bubble.  The bookmark bubble is a
// bubble that pops up when clicking on the STAR next to the URL to
// add or remove it as a bookmark.  This bubble allows for editing of
// the bookmark in various ways (name, folder, etc.)
//
// The bubble is stored in a nib as a view, not as a window, so we can
// make it an actual bubble.  There is no nib-rific way to encode a
// NSBorderlessWindowMask NSWindow, and the style of an NSWindow can't
// be set other than init time.  To deal, we create the NSWindow
// programatically, but encode the view in a nib.  Thus,
// BookmarkBubbleController is an NSViewController, not an
// NSWindowController.
@interface BookmarkBubbleController : NSViewController {
 @private
  // Unexpected for this controller, perhaps, but our window does NOT
  // come from a nib.
  scoped_nsobject<NSWindow> window_;

  id<BookmarkBubbleControllerDelegate> delegate_;  // weak like other delegates
  NSWindow* parentWindow_;  // weak
  NSPoint topLeftForBubble_;

  // Both weak; owned by the current browser's profile
  BookmarkModel* model_;
  const BookmarkNode* node_;

  // A mapping from titles to nodes so we only have to walk this once.
  scoped_nsobject<NSMutableArray> titleMapping_;

  BOOL alreadyBookmarked_;
  scoped_nsobject<NSString> chooseAnotherFolder_;

  IBOutlet NSTextField* bigTitle_;   // "Bookmark" or "Bookmark Added!"
  IBOutlet NSTextField* nameTextField_;
  IBOutlet NSComboBox* folderComboBox_;
}

// |node| is the bookmark node we edit in this bubble.
// |alreadyBookmarked| tells us if the node was bookmarked before the
//   user clicked on the star.  (if NO, this is a brand new bookmark).
// The owner of this object is responsible for showing the bubble if
// it desires it to be visible on the screen.  It is not shown by the
// init routine.  Closing of the window happens implicitly on dealloc.
- (id)initWithDelegate:(id<BookmarkBubbleControllerDelegate>)delegate
          parentWindow:(NSWindow*)parentWindow
      topLeftForBubble:(NSPoint)topLeftForBubble
                 model:(BookmarkModel*)model
                  node:(const BookmarkNode*)node
     alreadyBookmarked:(BOOL)alreadyBookmarked;

- (void)showWindow;

// Actions for buttons in the dialog.
- (IBAction)edit:(id)sender;
- (IBAction)close:(id)sender;
- (IBAction)remove:(id)sender;

@end


// Exposed only for unit testing.
@interface BookmarkBubbleController(ExposedForUnitTesting)
- (NSWindow*)createBubbleWindow;
- (void)fillInFolderList;
- (BOOL)windowHasBeenClosed;
- (void)addFolderNodes:(const BookmarkNode*)parent toComboBox:(NSComboBox*)box;
- (void)updateBookmarkNode;
- (void)setTitle:(NSString *)title parentFolder:(NSString*)folder;
- (NSString*)chooseAnotherFolderString;
@end

// Also private but I need to declare them specially for @synthesize to work.
@interface BookmarkBubbleController ()
@property (readonly) id delegate;
@property (readonly) NSComboBox* folderComboBox;
@end
