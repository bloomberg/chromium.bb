// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// Class for buttons that can be drag sources. If the mouse is clicked and moved
// more than a given distance, this class will call |-beginDrag:| instead of
// |-performClick:|. Subclasses should override these two methods.
@interface DraggableButton : NSButton {
 @private
  BOOL draggable_;     // Is this a draggable type of button?
  BOOL mayDragStart_;  // Set to YES on mouse down, NO on up or drag.
  BOOL beingDragged_;

  // Initial mouse-down to prevent a hair-trigger drag.
  NSPoint initialMouseDownLocation_;
}

// Enable or disable dragability for special buttons like "Other Bookmarks".
@property BOOL draggable;

// Called when a drag starts. Subclasses must override this.
- (void)beginDrag:(NSEvent*)dragEvent;

// Subclasses should call this method to notify DraggableButton when a drag is
// over.
- (void)endDrag;

@end  // @interface DraggableButton
