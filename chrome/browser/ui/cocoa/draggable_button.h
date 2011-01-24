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
}

// Enable or disable dragability for special buttons like "Other Bookmarks".
@property(nonatomic) BOOL draggable;

// Called when a drag should start. Subclasses must override this to do any
// pasteboard manipulation and begin the drag, usually with
// -dragImage:at:offset:event:.  Subclasses must call one of the blocking
// -drag* methods of NSView when overriding this method.
- (void)beginDrag:(NSEvent*)dragEvent;

@end  // @interface DraggableButton

@interface DraggableButton (Private)

// Resets the draggable state of the button after dragging is finished.  This is
// called by DraggableButton when the beginDrag call returns, it should not be
// called by the subclass.
- (void)endDrag;

@end  // @interface DraggableButton(Private)
