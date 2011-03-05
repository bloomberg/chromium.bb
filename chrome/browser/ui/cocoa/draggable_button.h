// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// Class for buttons that can be drag sources. If the mouse is clicked and moved
// more than a given distance, this class will call |-beginDrag:| instead of
// |-performClick:|. Subclasses should override these two methods.
@interface DraggableButton : NSButton {
 @private
  BOOL draggable_;        // Is this a draggable type of button?
  BOOL actionHasFired_;   // Has the action already fired for this click?
  BOOL actsOnMouseDown_;  // Does button action happen on mouse down when
                          // possible?
  NSTimeInterval durationMouseWasDown_;
  NSTimeInterval whenMouseDown_;
}

// Enable or disable dragability for special buttons like "Other Bookmarks".
@property(nonatomic) BOOL draggable;

// If it has a popup menu, for example, we want to perform the action on mouse
// down, if possible (as long as user still gets chance to drag, if
// appropriate).
@property(nonatomic) BOOL actsOnMouseDown;

// Called when a drag should start. Subclasses must override this to do any
// pasteboard manipulation and begin the drag, usually with
// -dragImage:at:offset:event:.  Subclasses must call one of the blocking
// -drag* methods of NSView when overriding this method.
- (void)beginDrag:(NSEvent*)dragEvent;

// Called internally. Default impl only returns YES if sender==self.
// Override if your subclass wants to accept being tracked into while a
// click is being tracked on another DraggableButton. Needed to support
// buttons being used as fake menu items or menu titles, as BookmarkButton does.
- (BOOL)acceptsTrackInFrom:(id)sender;

// Override if you want to do any extra work on mouseUp, after a mouseDown
// action has already fired.
- (void)secondaryMouseUpAction:(BOOL)wasInside;

// This is called internally.
// Decides if we now have enough information to stop tracking the mouse.
// It's the function below, deltaIndicatesDragStartWithXDelta. however, that
// decides whether it's a drag or not.
// Override if you want to do something tricky when making the decision.
// Default impl returns YES if ABS(xDelta) or ABS(yDelta) >= their respective
// hysteresis limit.
- (BOOL)deltaIndicatesConclusionReachedWithXDelta:(float)xDelta
                                           yDelta:(float)yDelta
                                      xHysteresis:(float)xHysteresis
                                      yHysteresis:(float)yHysteresis;

// This is called internally.
// Decides whether we should treat the click as a cue to start dragging, or
// instead call the mouseDown/mouseUp handler as appropriate.
// Override if you want to do something tricky when making the decision.
// Default impl returns YES if ABS(xDelta) or ABS(yDelta) >= their respective
// hysteresis limit.
- (BOOL)deltaIndicatesDragStartWithXDelta:(float)xDelta
                                   yDelta:(float)yDelta
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis;


@property(nonatomic) NSTimeInterval durationMouseWasDown;

@end  // @interface DraggableButton

@interface DraggableButton (Private)

// Resets the draggable state of the button after dragging is finished.  This is
// called by DraggableButton when the beginDrag call returns, it should not be
// called by the subclass.
- (void)endDrag;

// Called internally if the actsOnMouseDown property is set.
// Fires the button's action and tracks the click.
- (void)performMouseDownAction:(NSEvent*)theEvent;


@end  // @interface DraggableButton(Private)
