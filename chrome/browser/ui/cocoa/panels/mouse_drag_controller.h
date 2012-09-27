// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PANELS_MOUSE_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PANELS_MOUSE_DRAG_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

// When Drag is cancelled by hitting ESC key, we may still receive
// the mouseDragged events but should ignore them until the mouse button is
// released. Use these simple states to track this condition.
enum PanelDragState {
  PANEL_DRAG_CAN_START,  // Mouse key went down, drag may be started.
  PANEL_DRAG_IN_PROGRESS,
  PANEL_DRAG_SUPPRESSED  // Ignore drag events until PANEL_DRAG_CAN_START.
};

@class MouseDragController;

@protocol MouseDragControllerClient
// Called on initial mouseDown. Followed by dragStarted/dragProgress/dragEnded
// (which can be skipped if the drag didn't start) and then by cleanupAfterDrag.
- (void)prepareForDrag;

// Called wehen drag threshold was exceeded and actual drag should start.
// Note that the drag is processed using a local nested message loop.
// |initialMouseLocation| is in containing NSWindow's coordinates.
- (void)dragStarted:(NSPoint)initialMouseLocation;

// Called 0 to multiple times between dragStarted and dragEnded, to report
// current mouse location. |mouseLocation| is in window coordinates.
- (void)dragProgress:(NSPoint)mouseLocation;

- (void)dragEnded:(BOOL)cancelled;

// Always complements prepareForDrag. Clients which create a MouseDragController
// in their mouseDown may release it in this method.
- (void)cleanupAfterDrag;
@end

// This class encapsulates the mouse drag start/progress/termination logic,
// including having a threashold before actually starting a drag and termination
// of the drag on ESC, mouseUp and other operations. It also hosts the nested
// message loop that is used during the drag operation.
//
// The client of the controller should be a NSView implementing
// MouseDragControllerClient protocol. The client simply delegates mouse events
// to the controller, and controller invokes higher-level
// dragStarted/dragProgress/dragEnded callbacks on the client.
// The controller can be created in initial mouseDown and then released in the
// cleanupAfterDrag callback, or it can be preallocated and used across multiple
// drag operations.
//
// The pattern of usage:
// Lets say MyView is a NSView <MouseDragControllerClient>
//
// First, create an instance of MouseDragController and init it:
//     dragController_.reset([[MouseDragController alloc] initWithClient:self]);
// then delegate mouse messages to it:
//
// - (void)mouseDown:(NSEvent*)event {
//   if (needToStartADrag(event))
//     [dragController_ mouseDown:event];
// }
//
// - (void)mouseDragged:(NSEvent*)event {
//   [dragController_ mouseDragged:event];
// }
//
// - (void)mouseUp:(NSEvent*)event {
//   [dragController_ mouseUp:event];
// }
//
// That's it. When user starts a drag, the client's callbacks will be invoked.

@interface MouseDragController : NSObject {
 @private
   NSPoint initialMouseLocation_;  // In NSWindow's coordinate system.
   PanelDragState dragState_;
   NSView<MouseDragControllerClient>* client_;  // Weak, owns this object.
};

- (MouseDragController*)
    initWithClient:(NSView<MouseDragControllerClient>*)client;

// Accessors.
- (NSView<MouseDragControllerClient>*)client;
- (NSPoint)initialMouseLocation;

// These should be called from corresponding methods of hosting NSView.
- (void)mouseDown:(NSEvent*)event;
- (void)mouseDragged:(NSEvent*)event;
- (void)mouseUp:(NSEvent*)event;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PANELS_MOUSE_DRAG_CONTROLLER_H_
