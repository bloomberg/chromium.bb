// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "chrome/browser/cocoa/background_gradient_view.h"

@protocol BookmarkButtonControllerProtocol;

// Main content view for a bookmark bar folder "menu" window.  This is
// logically similar to a BookmarkBarView but is oriented vertically.
@interface BookmarkBarFolderView : NSView {
 @private
  BOOL inDrag_;  // Are we in the middle of a drag?
  BOOL dropIndicatorShown_;
  CGFloat dropIndicatorPosition_;  // y position
}
// Return the controller that owns this view.
- (id<BookmarkButtonControllerProtocol>)controller;
@end

@interface BookmarkBarFolderView(TestingAPI)
- (void)setDropIndicatorShown:(BOOL)shown;
@end

