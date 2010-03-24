// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_folder_target.h"

#include "base/logging.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/event_utils.h"

@implementation BookmarkFolderTarget

- (id)initWithController:(id<BookmarkButtonControllerProtocol>)controller {
  if ((self = [super init])) {
    controller_ = controller;
  }
  return self;
}

// This IBAction is called when the user clicks (mouseUp, really) on a
// "folder" bookmark button.  (In this context, "Click" does not
// include right-click to open a context menu which follows a
// different path).  Scenarios when folder X is clicked:
//  *Predicate*        *Action*
//  (nothing)          Open Folder X
//  Folder X open      Close folder X
//  Folder Y open      Close Y, open X
//  Cmd-click          Open All with proper disposition
//
//  Note complication in which a click-drag engages drag and drop, not
//  a click-to-open.  Thus the path to get here is a little twisted.
- (IBAction)openBookmarkFolderFromButton:(id)sender {
  DCHECK(sender);
  BOOL same = false;

  if ([controller_ folderController]) {
    // closeAllBookmarkFolders sets folderController_ to nil
    // so we need the SAME check to happen first.
    same = ([[controller_ folderController] parentButton] == sender);
    [controller_ closeAllBookmarkFolders];
  }

  // Watch out for a modifier click.  For example, command-click
  // should open all.
  //
  // NOTE: we cannot use [[sender cell] mouseDownFlags] because we
  // thwart the normal mouse click mechanism to make buttons
  // draggable.  Thus we must use [NSApp currentEvent].
  DCHECK([sender bookmarkNode]->is_folder());
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (disposition == NEW_BACKGROUND_TAB) {
    [controller_ openBookmarkNodesRecursive:[sender bookmarkNode]
                                disposition:disposition];
    return;
  }

  // If click on same folder, close it and be done.
  // Else we clicked on a different folder so more work to do.
  if (same)
    return;

  [controller_ addNewFolderControllerWithParentButton:sender];
}

@end
