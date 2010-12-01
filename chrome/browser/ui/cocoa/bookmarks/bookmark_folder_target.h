// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_FOLDER_TARGET_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_FOLDER_TARGET_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

@class BookmarkButton;
@protocol BookmarkButtonControllerProtocol;
class BookmarkNode;

// Target (in the target/action sense) of a bookmark folder button.
// Since ObjC doesn't have multiple inheritance we use has-a instead
// of is-a to share behavior between the BookmarkBarFolderController
// (NSWindowController) and the BookmarkBarController
// (NSViewController).
//
// This class is unit tested in the context of a BookmarkBarController.
@interface BookmarkFolderTarget : NSObject {
  // The owner of the bookmark folder button
  id<BookmarkButtonControllerProtocol> controller_;  // weak
}

- (id)initWithController:(id<BookmarkButtonControllerProtocol>)controller;

// Main IBAction for a button click.
- (IBAction)openBookmarkFolderFromButton:(id)sender;

// Copies the given bookmark node to the given pasteboard, declaring appropriate
// types (to paste a URL with a title).
- (void)copyBookmarkNode:(const BookmarkNode*)node
            toPasteboard:(NSPasteboard*)pboard;

// Fill the given pasteboard with appropriate data when the given button is
// dragged. Since the delegate has no way of providing pasteboard data later,
// all data must actually be put into the pasteboard and not merely promised.
- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button;

@end

// The (internal) |NSPasteboard| type string for bookmark button drags, used for
// dragging buttons around the bookmark bar. The data for this type is just a
// pointer to the |BookmarkButton| being dragged.
extern NSString* kBookmarkButtonDragType;

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_FOLDER_TARGET_CONTROLLER_H_
