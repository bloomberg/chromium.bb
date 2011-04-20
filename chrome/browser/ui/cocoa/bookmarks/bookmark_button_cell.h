// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BUTTON_CELL_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BUTTON_CELL_H_
#pragma once

#import "base/mac/cocoa_protocols.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"

class BookmarkNode;

// A button cell that handles drawing/highlighting of buttons in the
// bookmark bar.  This cell forwards mouseEntered/mouseExited events
// to its control view so that pseudo-menu operations
// (e.g. hover-over to open) can be implemented.
@interface BookmarkButtonCell : GradientButtonCell<NSMenuDelegate> {
 @private
  BOOL empty_;  // is this an "empty" button placeholder button cell?

  // Starting index of bookmarkFolder children that we care to use.
  int startingChildIndex_;

  // Should we draw the folder arrow as needed?  Not used for the bar
  // itself but used on the folder windows.
  BOOL drawFolderArrow_;

  // Arrow for folders
  scoped_nsobject<NSImage> arrowImage_;
}

@property(nonatomic, readwrite, assign) const BookmarkNode* bookmarkNode;
@property(nonatomic, readwrite, assign) int startingChildIndex;
@property(nonatomic, readwrite, assign) BOOL drawFolderArrow;

// Create a button cell which draws with a theme.
+ (id)buttonCellForNode:(const BookmarkNode*)node
            contextMenu:(NSMenu*)contextMenu
               cellText:(NSString*)cellText
              cellImage:(NSImage*)cellImage;

// Initialize a button cell which draws with a theme.
// Designated initializer.
- (id)initForNode:(const BookmarkNode*)node
      contextMenu:(NSMenu*)contextMenu
         cellText:(NSString*)cellText
        cellImage:(NSImage*)cellImage;

- (BOOL)empty;  // returns YES if empty.
- (void)setEmpty:(BOOL)empty;

// |-setBookmarkCellText:image:| is used to set the text and image of
// a BookmarkButtonCell, and align the image to the left (NSImageLeft)
// if there is text in the title, and centered (NSImageCenter) if
// there is not.  If |title| is nil, do not reset the title.
- (void)setBookmarkCellText:(NSString*)title
                      image:(NSImage*)image;

// Set the color of text in this cell.
- (void)setTextColor:(NSColor*)color;

- (BOOL)isFolderButtonCell;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BUTTON_CELL_H_
