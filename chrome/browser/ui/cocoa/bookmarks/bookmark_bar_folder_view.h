// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@protocol BookmarkButtonControllerProtocol;
@class BookmarkBarFolderController;

// Main content view for a bookmark bar folder "menu" window.  This is
// logically similar to a BookmarkBarView but is oriented vertically.
@interface BookmarkBarFolderView : NSView {
 @private
  BOOL inDrag_;  // Are we in the middle of a drag?
  BOOL dropIndicatorShown_;
  CGFloat dropIndicatorPosition_;  // y position
  // The following |controller_| is weak; used for testing only. See the imple-
  // mentation comment for - (id<BookmarkButtonControllerProtocol>)controller.
  BookmarkBarFolderController* controller_;
}
// Return the controller that owns this view.
- (id<BookmarkButtonControllerProtocol>)controller;
@end

@interface BookmarkBarFolderView()  // TestingOrInternalAPI
@property(assign) BOOL dropIndicatorShown;
@property(readonly) CGFloat dropIndicatorPosition;
- (void)setController:(id)controller;
@end
