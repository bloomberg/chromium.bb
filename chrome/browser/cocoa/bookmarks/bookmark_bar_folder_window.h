// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARKS_BOOKMARK_BAR_FOLDER_WINDOW_H_
#define CHROME_BROWSER_COCOA_BOOKMARKS_BOOKMARK_BAR_FOLDER_WINDOW_H_
#pragma once

#import <Cocoa/Cocoa.h>
#import "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"


// Window for a bookmark folder "menu".  This menu pops up when you
// click on a bookmark button that represents a folder of bookmarks.
// This window is borderless.
@interface BookmarkBarFolderWindow : NSWindow
@end

// Content view for the above window.  "Stock" other than the drawing
// of rounded corners.  Only used in the nib.
@interface BookmarkBarFolderWindowContentView : NSView {
  // Arrows to show ability to scroll up and down as needed.
  scoped_nsobject<NSImage> arrowUpImage_;
  scoped_nsobject<NSImage> arrowDownImage_;
}
@end

// Scroll view that contains the main view (where the buttons go).
@interface BookmarkBarFolderWindowScrollView : NSScrollView
@end


#endif  // CHROME_BROWSER_COCOA_BOOKMARKS_BOOKMARK_BAR_FOLDER_WINDOW_H_
