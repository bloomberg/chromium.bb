// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BOOKMARK_BUTTON_CELL_H_
#define CHROME_BROWSER_COCOA_BOOKMARK_BUTTON_CELL_H_

#import "base/cocoa_protocols_mac.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"

// A button cell that handles drawing/highlighting of buttons in the
// bookmark bar.

@interface BookmarkButtonCell : GradientButtonCell<NSMenuDelegate> {
}
// |-setBookmarkCellText:image:| is used to set the text and image
// of a BookmarkButtonCell, and align the image to the left (NSImageLeft)
// if there is text in the title, and centered (NSImageCenter) if there is
- (void)setBookmarkCellText:(NSString*)title
                      image:(NSImage*)image;
@end

#endif  // CHROME_BROWSER_COCOA_BOOKMARK_BUTTON_CELL_H_
