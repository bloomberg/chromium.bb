// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa.h"

class BookmarkBubbleNotificationBridge;
class BookmarkModel;
class BookmarkNode;
@class BookmarkBubbleController;
@class InfoBubbleView;


// Controller for the bookmark bubble.  The bookmark bubble is a
// bubble that pops up when clicking on the STAR next to the URL to
// add or remove it as a bookmark.  This bubble allows for editing of
// the bookmark in various ways (name, folder, etc.)
@interface BookmarkBubbleController : NSWindowController<NSWindowDelegate> {
 @private
  NSWindow* parentWindow_;  // weak

  // Both weak; owned by the current browser's profile
  BookmarkModel* model_;  // weak
  const BookmarkNode* node_;  // weak

  // The bookmark node whose button we asked to pulse.
  const BookmarkNode* pulsingBookmarkNode_;  // weak

  BOOL alreadyBookmarked_;

  // Ping me when the bookmark model changes out from under us.
  scoped_ptr<BookmarkModelObserverForCocoa> bookmark_observer_;

  // Ping me when other Chrome things change out from under us.
  scoped_ptr<BookmarkBubbleNotificationBridge> chrome_observer_;

  IBOutlet NSTextField* bigTitle_;   // "Bookmark" or "Bookmark Added!"
  IBOutlet NSTextField* nameTextField_;
  IBOutlet NSPopUpButton* folderPopUpButton_;
  IBOutlet InfoBubbleView* bubble_;  // to set arrow position
}

@property(readonly, nonatomic) const BookmarkNode* node;

// |node| is the bookmark node we edit in this bubble.
// |alreadyBookmarked| tells us if the node was bookmarked before the
//   user clicked on the star.  (if NO, this is a brand new bookmark).
// The owner of this object is responsible for showing the bubble if
// it desires it to be visible on the screen.  It is not shown by the
// init routine.  Closing of the window happens implicitly on dealloc.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                     model:(BookmarkModel*)model
                      node:(const BookmarkNode*)node
         alreadyBookmarked:(BOOL)alreadyBookmarked;

// Actions for buttons in the dialog.
- (IBAction)ok:(id)sender;
- (IBAction)remove:(id)sender;
- (IBAction)cancel:(id)sender;

// These actions send a -editBookmarkNode: action up the responder chain.
- (IBAction)edit:(id)sender;
- (IBAction)folderChanged:(id)sender;

@end


// Exposed only for unit testing.
@interface BookmarkBubbleController(ExposedForUnitTesting)
- (void)addFolderNodes:(const BookmarkNode*)parent
         toPopUpButton:(NSPopUpButton*)button
           indentation:(int)indentation;
- (void)setTitle:(NSString*)title parentFolder:(const BookmarkNode*)parent;
- (void)setParentFolderSelection:(const BookmarkNode*)parent;
+ (NSString*)chooseAnotherFolderString;
- (NSPopUpButton*)folderPopUpButton;
@end
