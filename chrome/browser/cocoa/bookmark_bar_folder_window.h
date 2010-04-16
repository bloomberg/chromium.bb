// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BAR_FOLDER_WINDOW_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BAR_FOLDER_WINDOW_H_

#import <Cocoa/Cocoa.h>

// Window for a bookmark folder "menu".  This menu pops up when you
// click on a bookmark button that represents a folder of bookmarks.
// This window is borderless.
@interface BookmarkBarFolderWindow : NSWindow
@end


// Content view for the above window.  "Stock" other than the drawing
// of rounded corners.  Only used in the nib.
@interface BookmarkBarFolderWindowContentView : NSView
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BAR_FOLDER_WINDOW_H_
