// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_FOLDER_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_FOLDER_BUTTON_CELL_H_
#pragma once

#import "chrome/browser/cocoa/bookmark_button_cell.h"

class BookmarkNode;

// A button cell that handles drawing/highlighting of buttons in the
// bookmark bar.  This cell forwards mouseEntered/mouseExited events
// to its control view so that pseudo-menu operations
// (e.g. hover-over to open) can be implemented.
@interface BookmarkBarFolderButtonCell : BookmarkButtonCell {
 @private
  scoped_nsobject<NSColor> frameColor_;
}

// Create a button cell which draws without a theme and with a frame
// color provided by the BrowserThemeProvider defaults.
+ (id)buttonCellForNode:(const BookmarkNode*)node
            contextMenu:(NSMenu*)contextMenu
               cellText:(NSString*)cellText
              cellImage:(NSImage*)cellImage;

@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_FOLDER_BUTTON_CELL_H_
