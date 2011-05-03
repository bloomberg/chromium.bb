// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_button_cell.h"

@implementation BookmarkBarFolderButtonCell

+ (id)buttonCellForNode:(const BookmarkNode*)node
            contextMenu:(NSMenu*)contextMenu
               cellText:(NSString*)cellText
              cellImage:(NSImage*)cellImage {
  id buttonCell =
      [[[BookmarkBarFolderButtonCell alloc] initForNode:node
                                            contextMenu:contextMenu
                                               cellText:cellText
                                              cellImage:cellImage]
       autorelease];
  return buttonCell;
}

- (BOOL)isFolderButtonCell {
  return YES;
}

- (void)setMouseInside:(BOOL)flag animate:(BOOL)animated {
}

@end
