// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@protocol BookmarkButtonDelegate;

// Class for bookmark bar buttons that can be drag sources.
@interface BookmarkButton : NSButton {
 @private
  BOOL draggable_;     // Is this a draggable type of button?
  BOOL mayDragStart_;  // Set to YES on mouse down, NO on up or drag.
  BOOL beingDragged_;
  id<BookmarkButtonDelegate> delegate_;

  // Initial mouse-down to prevent a hair-trigger drag.
  NSPoint initialMouseDownLocation_;
}

// Enable or disable dragability for special buttons like "Other Bookmarks".
@property BOOL draggable;

@property(assign, nonatomic) id<BookmarkButtonDelegate> delegate;

@end  // @interface BookmarkButton

// Protocol for a |BookmarkButton|'s delegate, which is responsible for doing
// things which require information about the bookmark represented by this
// button.
@protocol BookmarkButtonDelegate

// Fill the given pasteboard with appropriate data when the given button is
// dragged. Since the delegate has no way of providing pasteboard data later,
// all data must actually be put into the pasteboard and not merely promised.
- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button;

@end  // @protocol BookmarkButtonDelegate
