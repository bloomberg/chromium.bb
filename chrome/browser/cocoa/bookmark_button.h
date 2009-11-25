// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// Class for bookmark bar buttons that can be drag sources.
@interface BookmarkButton : NSButton {
 @private
  BOOL draggable_;     // Is this a draggable type of button?
  BOOL mayDragStart_;  // Set to YES on mouse down, NO on up or drag.
  BOOL beingDragged_;

  // Initial mouse-down to prevent a hair-trigger drag.
  NSPoint initialMouseDownLocation_;
}

// Enable or disable dragability for special buttons like "Other Bookmarks".
@property BOOL draggable;

@end

extern NSString* kBookmarkButtonDragType;

