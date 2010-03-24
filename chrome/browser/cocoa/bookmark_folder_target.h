// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_FOLDER_TARGET_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_FOLDER_TARGET_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

@protocol BookmarkButtonControllerProtocol;

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

@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_FOLDER_TARGET_CONTROLLER_H_
